



#include "gdrive-cache-node.h"
#include "gdrive-cache.h"

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_Cache_Node
{
    time_t lastUpdateTime;
    int openCount;
    int openWrites;
    bool dirty;
    bool deleted;
    Gdrive_Fileinfo fileinfo;
    Gdrive_File_Contents* pContents;
    struct Gdrive_Cache_Node* pParent;
    struct Gdrive_Cache_Node* pLeft;
    struct Gdrive_Cache_Node* pRight;
} Gdrive_Cache_Node;

static Gdrive_Cache_Node* gdrive_cnode_create(Gdrive_Cache_Node* pParent);

static void
gdrive_cnode_swap(Gdrive_Cache_Node** ppFromParentOne,
                        Gdrive_Cache_Node* pNodeOne,
                        Gdrive_Cache_Node** ppFromParentTwo,
                        Gdrive_Cache_Node* pNodeTwo
);

static void
gdrive_cnode_free(Gdrive_Cache_Node* pNode);

static Gdrive_File_Contents* 
gdrive_cnode_add_contents(Gdrive_Cache_Node* pNode);

static Gdrive_File_Contents* 
gdrive_cnode_create_chunk(Gdrive_Cache_Node* pNode, off_t offset, size_t size, 
                          bool fillChunk);

static size_t 
gdrive_file_read_next_chunk(Gdrive_File* pNode, char* destBuf, off_t offset,
                            size_t size);

static off_t 
gdrive_file_write_next_chunk(Gdrive_File* pFile, const char* buf, off_t offset, 
                             size_t size);

static bool 
gdrive_file_check_perm(const Gdrive_Cache_Node* pNode, int accessFlags);

static size_t 
gdrive_file_uploadcallback(char* buffer, off_t offset, size_t size, 
                           void* userdata);

static char* 
gdrive_file_sync_metadata_or_create(Gdrive_Fileinfo* pFileinfo, 
                                    const char* parentId, const char* filename,
                                    bool isFolder, int* pError);



/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors and destructors
 ******************/

Gdrive_Cache_Node* gdrive_cnode_get(Gdrive_Cache_Node* pParent,
                                    Gdrive_Cache_Node** ppNode,
                                    const char* fileId,
                                    bool addIfDoesntExist,
                                    bool* pAlreadyExists
)
{
    if (pAlreadyExists != NULL)
    {
        *pAlreadyExists = false;
    }
    
    if (*ppNode == NULL)
    {
        // Item doesn't exist in the cache. Either fail, or create a new item.
        if (!addIfDoesntExist)
        {
            // Not allowed to create a new item, return failure.
            return NULL;
        }
        // else create a new item.
        *ppNode = gdrive_cnode_create(pParent);
        if (*ppNode == NULL)
        {
            // Memory error
            return NULL;
        }
        // Convenience to avoid things like "return &((*ppNode)->fileinfo);"
        Gdrive_Cache_Node* pNode = *ppNode;
        
        // Get the fileinfo
        char* url = malloc(strlen(GDRIVE_URL_FILES) + strlen(fileId) + 2);
        if (!url)
        {
            // Memory error
            return NULL;
        }
        strcpy(url, GDRIVE_URL_FILES);
        strcat(url, "/");
        strcat(url, fileId);
        Gdrive_Transfer* pTransfer = gdrive_xfer_create();
        if (!pTransfer)
        {
            // Memory error
            return NULL;
        }
        if (gdrive_xfer_set_url(pTransfer, url))
        {
            // Error, probably memory
            free(url);
            return NULL;
        }
        free(url);
        gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_GET);
        Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
        gdrive_xfer_free(pTransfer);
        if (!pBuf || gdrive_dlbuf_get_httpResp(pBuf) >= 400)
        {
            // Download or request error
            free(pBuf);
            return NULL;
        }
        Gdrive_Json_Object* pObj = 
                gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
        gdrive_dlbuf_free(pBuf);
        if (!pObj)
        {
            // Couldn't convert network response to JSON
            return NULL;
        }
        gdrive_cnode_update_from_json(pNode, pObj);
        gdrive_json_kill(pObj);

        return pNode;
    }
    
    // Convenience to avoid things like "&((*ppNode)->pRight)"
    Gdrive_Cache_Node* pNode = *ppNode;
    
    // Root node exists, try to find the fileId in the tree.
    if (!pNode->fileinfo.id)
    {
        puts("NULL File ID");
    }
    int cmp = strcmp(fileId, pNode->fileinfo.id);
    if (cmp == 0)
    {
        // Found it at the current node.
        if (pAlreadyExists != NULL)
        {
            *pAlreadyExists = true;
        }
        return pNode;
    }
    else if (cmp < 0)
    {
        // fileId is less than the current node. Look for it on the left.
        return gdrive_cnode_get(pNode, &(pNode->pLeft), fileId, 
                                      addIfDoesntExist, pAlreadyExists
                );
    }
    else
    {
        // fileId is greater than the current node. Look for it on the right.
        return gdrive_cnode_get(pNode, &(pNode->pRight), fileId, 
                                      addIfDoesntExist, pAlreadyExists
                );
    }
}


void gdrive_cnode_delete(Gdrive_Cache_Node* pNode, 
                               //Gdrive_Cache_Node* pParentNode,
                               Gdrive_Cache_Node** ppToRoot
)
{
    // The address of the pointer aimed at this node. If this is the root node,
    // then it will be a pointer passed in from outside. Otherwise, it is the
    // pLeft or pRight member of the parent.
    Gdrive_Cache_Node** ppFromParent;
    if (pNode->pParent == NULL)
    {
        // This is the root. Take the pointer that was passed in.
        ppFromParent = ppToRoot;
    }
    else
    {
        // Not the root. Find whether the node hangs from the left or right side
        // of its parent.
        assert(pNode->pParent->pLeft == pNode || pNode->pParent->pRight == pNode);
        ppFromParent = (pNode->pParent->pLeft == pNode) ?
            &(pNode->pParent->pLeft) : &(pNode->pParent->pRight);
        
    }
    
    // Simplest special case. pNode has no descendents.  Just delete it, and
    // set the pointer from the parent to NULL.
    if (pNode->pLeft == NULL && pNode->pRight == NULL)
    {
        *ppFromParent = NULL;
        gdrive_cnode_free(pNode);
        return;
    }
    
    // Second special case. pNode has one side empty. Promote the descendent on
    // the other side into pNode's place.
    if (pNode->pLeft == NULL)
    {
        *ppFromParent = pNode->pRight;
        pNode->pRight->pParent = pNode->pParent;
        gdrive_cnode_free(pNode);
        return;
    }
    if (pNode->pRight == NULL)
    {
        *ppFromParent = pNode->pLeft;
        pNode->pLeft->pParent = pNode->pParent;
        gdrive_cnode_free(pNode);
        return;
    }
    
    // General case with descendents on both sides. Find the node with the 
    // closest value to pNode in one of its subtrees (leftmost node of the right
    // subtree, or rightmost node of the left subtree), and switch places with
    // pNode.  Which side we use doesn't really matter.  We'll rather 
    // arbitrarily decide to use the same side subtree as the side from which
    // pNode hangs off its parent (if pNode is on the right side of its parent,
    // find the leftmost node of the right subtree), and treat the case where
    // pNode is the root the same as if it were on the left side of its parent.
    Gdrive_Cache_Node* pSwap = NULL;
    Gdrive_Cache_Node** ppToSwap = NULL;
    if (pNode->pParent != NULL && pNode->pParent->pRight == pNode)
    {
        // Find the leftmost node of the right subtree.
        pSwap = pNode->pRight;
        ppToSwap = &(pNode->pRight);
        while (pSwap->pLeft != NULL)
        {
            ppToSwap = &(pSwap->pLeft);
            pSwap = pSwap->pLeft;
        }
    }
    else
    {
        // Find the rightmost node of the left subtree.
        pSwap = pNode->pLeft;
        ppToSwap = &(pNode->pLeft);
        while (pSwap->pRight != NULL)
        {
            ppToSwap = &(pSwap->pRight);
            pSwap = pSwap->pRight;
        }
    }
    
    // Swap the nodes
    gdrive_cnode_swap(ppFromParent, pNode, ppToSwap, pSwap);
    
    // Now delete the node from its new position.
    gdrive_cnode_delete(pNode, ppToRoot);
}

void gdrive_cnode_mark_deleted(Gdrive_Cache_Node* pNode, 
                               Gdrive_Cache_Node** ppToRoot
)
{
    pNode->deleted = true;
    if (pNode->openCount == 0)
    {
        gdrive_cnode_delete(pNode, ppToRoot);
    }
}

void gdrive_cnode_free_all(Gdrive_Cache_Node* pRoot)
{
    if (pRoot == NULL)
    {
        // Nothing to do.
        return;
    }
    
    // Free all the descendents first.
    gdrive_cnode_free_all(pRoot->pLeft);
    gdrive_cnode_free_all(pRoot->pRight);
    
    // Free the root node
    gdrive_cnode_free(pRoot);
}


/******************
 * Getter and setter functions
 ******************/

time_t gdrive_cnode_get_update_time(Gdrive_Cache_Node* pNode)
{
    return pNode->lastUpdateTime;
}

enum Gdrive_Filetype gdrive_cnode_get_filetype(Gdrive_Cache_Node* pNode)
{
    return pNode->fileinfo.type;
}

Gdrive_Fileinfo* gdrive_cnode_get_fileinfo(Gdrive_Cache_Node* pNode)
{
    return &(pNode->fileinfo);
}




/******************
 * Other accessible functions
 ******************/

void gdrive_cnode_update_from_json(Gdrive_Cache_Node* pNode, 
                                       Gdrive_Json_Object* pObj
)
{
    if (pNode == NULL || pObj == NULL)
    {
        // Nothing to do
        return;
    }
    gdrive_finfo_cleanup(&(pNode->fileinfo));
    gdrive_finfo_read_json(&(pNode->fileinfo), pObj);
    
    // Mark the node as having been updated.
    pNode->lastUpdateTime = time(NULL);
}

void gdrive_cnode_delete_file_contents(Gdrive_Cache_Node* pNode, 
                                Gdrive_File_Contents* pContents
)
{
    gdrive_fcontents_delete(pContents, &(pNode->pContents));
}

bool gdrive_cnode_is_dirty(const Gdrive_Cache_Node* pNode)
{
    return pNode->dirty;
}

bool gdrive_cnode_isdeleted(const Gdrive_Cache_Node* pNode)
{
    assert(pNode != NULL);
    return pNode->deleted;
}


/*************************************************************************
 * Public functions to support Gdrive_File usage
 *************************************************************************/

Gdrive_File* gdrive_file_open(const char* fileId, int flags, int* pError)
{
    assert(fileId != NULL && pError != NULL);
    
    // Get the cache node from the cache if it exists.  If it doesn't exist,
    // don't make a node with an empty Gdrive_Fileinfo.  Instead, use 
    // gdrive_file_info_from_id() to create the node and fill out the struct, 
    // then try again to get the node.
    Gdrive_Cache_Node* pNode;
    while ((pNode = gdrive_cache_get_node(fileId, false, NULL)) == NULL)
    {
        if (gdrive_finfo_get_by_id(fileId) == NULL)
        {
            // Problem getting the file info.  Return failure.
            *pError = ENOENT;
            return NULL;
        }
    }
    
    // If the file is deleted, existing filehandles will still work, but nobody
    // new can open it.
    if (gdrive_cnode_isdeleted(pNode))
    {
        *pError = ENOENT;
        return NULL;
    }
    
    // Don't open directories, only regular files.
    if (pNode->fileinfo.type == GDRIVE_FILETYPE_FOLDER)
    {
        // Return failure
        *pError = EISDIR;
        return NULL;
    }
    
    
    if (!gdrive_file_check_perm(pNode, flags))
    {
        // Access error
        *pError = EACCES;
        return NULL;
    }
    
    
    // Increment the open counter
    pNode->openCount++;
    
    if ((flags & O_WRONLY) || (flags & O_RDWR))
    {
        // Open for writing
        pNode->openWrites++;
    }
    
    // Return a pointer to the cache node (which is typedef'ed to 
    // Gdrive_Filehandle)
    return pNode;
    
}

void gdrive_file_close(Gdrive_File* pFile, int flags)
{
    assert(pFile != NULL);
    
    // Gdrive_Filehandle and Gdrive_Cache_Node are the same thing, but it's 
    // easier to think of the filehandle as just a token used to refer to a 
    // file, whereas a cache node has internal structure to act upon.
    Gdrive_Cache_Node* pNode = pFile;
    
    if ((flags & O_WRONLY) || (flags & O_RDWR))
    {
        // Was opened for writing
        
        // Upload any changes back to Google Drive
        gdrive_file_sync(pFile);
        gdrive_file_sync_metadata(pFile);
        
        // Close the file
        pNode->openWrites--;
    }
    
    // Decrement open file counts.
    pNode->openCount--;
    
    
    // Get rid of any downloaded temp files if they aren't needed.
    // TODO: Consider keeping some closed files around in case they're reopened
    if (pNode->openCount == 0)
    {
        gdrive_fcontents_free_all(&(pNode->pContents));
        if (gdrive_cnode_isdeleted(pNode))
        {
            gdrive_cache_delete_node(pNode);
        }
    }
}

int gdrive_file_read(Gdrive_File* fh, char* buf, size_t size, off_t offset)
{
    assert(fh != NULL && offset >= (off_t) 0);
    
    // Make sure we have at least read access for the file.
    if (!gdrive_file_check_perm(fh, O_RDONLY))
    {
        // Access error
        return -EACCES;
    }
    
    off_t nextOffset = offset;
    off_t bufferOffset = 0;
    
    // Starting offset must be within the file
    if (offset >= (off_t) fh->fileinfo.size)
    {
        return 0;
    }
    
    // Don't read past the current file size
    size_t realSize = (size + offset <= fh->fileinfo.size)? 
        size : fh->fileinfo.size - offset;
    
    size_t bytesRemaining = realSize;
    while (bytesRemaining > 0)
    {
        // Read into the current position if we're given a real buffer, or pass
        // in NULL otherwise
        char* bufPos = (buf != NULL) ? buf + bufferOffset : NULL;
        off_t bytesRead = gdrive_file_read_next_chunk(fh, 
                                                      bufPos,
                                                      nextOffset, 
                                                      bytesRemaining
                );
        if (bytesRead < 0)
        {
            // Read error.  bytesRead is the negative error number
            return bytesRead;
        }
        if (bytesRead == 0)
        {
            // EOF. Return the total number of bytes actually read.
            return realSize - bytesRemaining;
        }
        nextOffset += bytesRead;
        bufferOffset += bytesRead;
        bytesRemaining -= bytesRead;
    }
    
    return realSize;
}

int gdrive_file_write(Gdrive_File* fh, 
                      const char* buf, 
                      size_t size, 
                      off_t offset
)
{
    assert(fh != NULL);
    
    // Make sure we have read and write access for the file.
    if (!gdrive_file_check_perm(fh, O_RDWR))
    {
        // Access error
        return -EACCES;
    }
    
    // Read any needed chunks into the cache.
    off_t readOffset = offset;
    size_t readSize = size;
    if (offset == (off_t) gdrive_file_get_info(fh)->size)
    {
        if (readOffset > 0) readOffset--;
        readSize++;
    }
    gdrive_file_read(fh, NULL, readSize, readOffset);
    
    off_t nextOffset = offset;
    off_t bufferOffset = 0;
    size_t bytesRemaining = size;
    
    while (bytesRemaining > 0)
    {
        off_t bytesWritten = gdrive_file_write_next_chunk(fh, 
                                                          buf + bufferOffset,
                                                          nextOffset, 
                                                          bytesRemaining
                );
        if (bytesWritten < 0)
        {
            // Write error.  bytesWritten is the negative error number
            return bytesWritten;
        }
        nextOffset += bytesWritten;
        bufferOffset += bytesWritten;
        bytesRemaining -= bytesWritten;
    }
    
    return size;
}

int gdrive_file_truncate(Gdrive_File* fh, off_t size)
{
    assert(fh != NULL);
    
    // 4 possible cases:
    //      A. size is current size
    //      B. size is 0
    //      C. size is greater than current size
    //      D. size is less than current size
    // A and B are special cases. C and D share some similarities with each 
    // other.
    
    // Check for write permissions
    if (!gdrive_file_check_perm(fh, O_RDWR))
    {
        return -EACCES;
    }
    
    // Case A: Do nothing, return success.
    if (fh->fileinfo.size == (size_t) size)
    {
        return 0;
    }
    
    // Case B: Delete all cached file contents, set the length to 0.
    if (size == 0)
    {
        gdrive_fcontents_free_all(&(fh->pContents));
        fh->fileinfo.size = 0;
        fh->dirty = true;
        return 0;
    }
    
    // Cases C and D: Identify the final chunk (or the chunk that will become 
    // final, make sure it is cached, and  truncate it. Afterward, set the 
    // file's length.
    
    Gdrive_File_Contents* pFinalChunk = NULL;
    if ((size_t) size > fh->fileinfo.size)
    {
        // File is being lengthened. The current final chunk will remain final.
        if (fh->fileinfo.size > 0)
        {
            // If the file is non-zero length, read the last byte of the file to
            // cache it.
            if (gdrive_file_read(fh, NULL, 1, fh->fileinfo.size - 1) < 0)
            {
                // Read error
                return -EIO;
            }
            
            // Grab the final chunk
            pFinalChunk = gdrive_fcontents_find_chunk(fh->pContents, 
                                                      fh->fileinfo.size - 1
                    );
        }
        else
        {
            // The file is zero-length to begin with. If a chunk exists, use it,
            // but we'll probably need to create one.
            if ((pFinalChunk = gdrive_fcontents_find_chunk(fh->pContents, 0))
                    == NULL)
            {
                pFinalChunk = gdrive_cnode_create_chunk(fh, 0, size, false);
            }
        }
    }
    else
    {
        // File is being shortened.
        
        // The (new) final chunk is the one that contains what will become the
        // last byte of the truncated file. Read this byte in order to cache the
        // chunk.
        if (gdrive_file_read(fh, NULL, 1, size - 1) < 0)
        {
            // Read error
            return -EIO;
        }
        
        // Grab the final chunk
        pFinalChunk = gdrive_fcontents_find_chunk(fh->pContents, size - 1);
        
        // Delete any chunks past the new EOF
        gdrive_fcontents_delete_after_offset(&(fh->pContents), size - 1);
    }
    
    // Make sure we received the final chunk
    if (pFinalChunk == NULL)
    {
        // Error
        return -EIO;
    }
    
    int returnVal = gdrive_fcontents_truncate(pFinalChunk, size);
    
    if (returnVal == 0)
    {
        // Successfully truncated the chunk. Update the file's size.
        fh->fileinfo.size = size;
        fh->dirty = true;
    }
    
    return returnVal;
}

int gdrive_file_sync(Gdrive_File* fh)
{
    if (fh == NULL)
    {
        // Invalid argument
        return -EINVAL;
    }
    
    Gdrive_Cache_Node* pNode = fh;
    
    if (!pNode->dirty)
    {
        // Nothing to do
        return 0;
    }
    
    // Just using simple upload for now.
    // TODO: Consider using resumable upload, possibly only for large files.
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        // Memory error
        return -ENOMEM;
    }
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_PUT);
    
    // Assemble the URL
    size_t urlSize = strlen(GDRIVE_URL_UPLOAD) + strlen(pNode->fileinfo.id) + 2;
    char* url = malloc(urlSize);
    if (url == NULL)
    {
        // Memory error
        gdrive_xfer_free(pTransfer);
        return -ENOMEM;
    }
    strcpy(url, GDRIVE_URL_UPLOAD);
    strcat(url, "/");
    strcat(url, pNode->fileinfo.id);
    if (gdrive_xfer_set_url(pTransfer, url) != 0)
    {
        // Error, probably memory
        free(url);
        gdrive_xfer_free(pTransfer);
        return -ENOMEM;
    }
    free(url);
    
    // Add query parameter(s)
    if (gdrive_xfer_add_query(pTransfer, "uploadType", "media") != 0)
    {
        // Error, probably memory
        gdrive_xfer_free(pTransfer);
        return -ENOMEM;
    }
    
    // Set upload callback
    gdrive_xfer_set_uploadcallback(pTransfer, gdrive_file_uploadcallback, fh);
    
    // Do the transfer
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    int returnVal = (pBuf == NULL || gdrive_dlbuf_get_httpResp(pBuf) >= 400);
    if (returnVal == 0)
    {
        // Success. Clear the dirty flag
        pNode->dirty = false;
    }
    gdrive_dlbuf_free(pBuf);
    return returnVal;
}

int gdrive_file_sync_metadata(Gdrive_File* fh)
{
    assert(fh != NULL);
    
    Gdrive_Cache_Node* pNode = fh;
    Gdrive_Fileinfo* pFileinfo = &(pNode->fileinfo);
    if (!pFileinfo->dirtyMetainfo)
    {
        // Nothing to sync, do nothing
        return 0;
    }
    
    int error = 0;
    char* dummy = 
        gdrive_file_sync_metadata_or_create(pFileinfo, NULL, NULL, 
                                            (pFileinfo->type == 
                                            GDRIVE_FILETYPE_FOLDER), 
                                            &error
    );
    free(dummy);
    return error;
}

int gdrive_file_set_atime(Gdrive_File* fh, const struct timespec* ts)
{
    assert(fh != NULL);
    
    Gdrive_Cache_Node* pNode = fh;

    // Make sure we have write permission
    if (!gdrive_file_check_perm(pNode, O_RDWR))
    {
        return -EACCES;
    }

    gdrive_finfo_set_atime(&(pNode->fileinfo), ts);
    return 0;
}

int gdrive_file_set_mtime(Gdrive_File* fh, const struct timespec* ts)
{
    assert(fh != NULL);
    
    Gdrive_Cache_Node* pNode = fh;
    
    // Make sure we have write permission
    if (!gdrive_file_check_perm(pNode, O_RDWR))
    {
        return -EACCES;
    }

    gdrive_finfo_set_mtime(&(pNode->fileinfo), ts);
    return 0;
}

char* gdrive_file_new(const char* path, bool createFolder, int* pError)
{
    assert(path != NULL && path[0] == '/' && pError != NULL);
            
    // Separate path into basename and parent folder.
    Gdrive_Path* pGpath = gdrive_path_create(path);
    if (pGpath == NULL)
    {
        // Memory error
        *pError = ENOMEM;
        return NULL;
    }
    const char* folderName = gdrive_path_get_dirname(pGpath);
    const char* filename = gdrive_path_get_basename(pGpath);
    
    // Check basename for validity (non-NULL, not a directory link such as "..")
    if (filename == NULL || filename[0] == '/' || 
            strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
    {
        *pError = EISDIR;
        gdrive_path_free(pGpath);
        return NULL;
    }
    
    // Check folder for validity (non-NULL, starts with '/', and is an existing
    // folder)
    if (folderName == NULL || folderName[0] != '/')
    {
        // Path wasn't in the form of an absolute path
        *pError = ENOTDIR;
        gdrive_path_free(pGpath);
        return NULL;
    }
    char* parentId = gdrive_filepath_to_id(folderName);
    if (parentId == NULL)
    {
        // Folder doesn't exist
        *pError = ENOTDIR;
        gdrive_path_free(pGpath);
        return NULL;
    }
    Gdrive_Cache_Node* pFolderNode = 
            gdrive_cache_get_node(parentId, true, NULL);
    if (pFolderNode == NULL)
    {
        // Couldn't get a node for the parent folder
        *pError = EIO;
        gdrive_path_free(pGpath);
        free(parentId);
        return NULL;
    }
    const Gdrive_Fileinfo* pFolderinfo = gdrive_cnode_get_fileinfo(pFolderNode);
    if (pFolderinfo == NULL || pFolderinfo->type != GDRIVE_FILETYPE_FOLDER)
    {
        // Not an actual folder
        *pError = ENOTDIR;
        gdrive_path_free(pGpath);
        free(parentId);
        return NULL;
    }
    
    // Make sure we have write access to the folder
    if (!gdrive_file_check_perm(pFolderNode, O_WRONLY))
    {
        // Don't have the needed permission
        *pError = EACCES;
        gdrive_path_free(pGpath);
        free(parentId);
        return NULL;
    }
    
    
    char* fileId = gdrive_file_sync_metadata_or_create(NULL, parentId, filename,
                                                       createFolder, pError);
    gdrive_path_free(pGpath);
    free(parentId);
    
    // TODO: See if gdrive_cache_add_fileid() can be modified to return a 
    // pointer to the cached ID (which is a new copy of the ID that was passed
    // in). This will avoid the need to look up the ID again after adding it,
    // and it will also help with multiple files that have identical paths.
    int result = gdrive_cache_add_fileid(path, fileId);
    free(fileId);
    if (result != 0)
    {
        // Probably a memory error
        *pError = ENOMEM;
        return NULL;
    }
    
    return gdrive_filepath_to_id(path);
}

Gdrive_Fileinfo* gdrive_file_get_info(Gdrive_File* fh)
{
//    if (fh == NULL)
//    {
//        // Invalid argument
//        return NULL;
//    }
    assert(fh != NULL);
    
    // No need to check permissions. This is just metadata, and metadata 
    // permissions were already needed just to access the filesystem and get a
    // filehandle in the first place.
    
    // Gdrive_Filehandle and Gdrive_Cache_Node are typedefs of the same struct,
    // but it's easier to think about them differently. A filehandle is a token,
    // and a cache node has an internal structure.
    Gdrive_Cache_Node* pNode = fh;
    return gdrive_cnode_get_fileinfo(pNode);
}

unsigned int gdrive_file_get_perms(const Gdrive_File* fh)
{
    const Gdrive_Cache_Node* pNode = fh;
    return gdrive_finfo_real_perms(&(pNode->fileinfo));
}



/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

/*
 * Set pParent to NULL for the root node of the tree (the node that has no
 * parent).
 */
static Gdrive_Cache_Node* gdrive_cnode_create(Gdrive_Cache_Node* pParent)
{
    Gdrive_Cache_Node* result = malloc(sizeof(Gdrive_Cache_Node));
    if (result != NULL)
    {
        memset(result, 0, sizeof(Gdrive_Cache_Node));
        result->pParent = pParent;
    }
    return result;
}

/*
 * pNodeTwo must be a descendent of pNodeOne, or neither node is descended from
 * the other.
 */
static void
gdrive_cnode_swap(Gdrive_Cache_Node** ppFromParentOne,
                        Gdrive_Cache_Node* pNodeOne,
                        Gdrive_Cache_Node** ppFromParentTwo,
                        Gdrive_Cache_Node* pNodeTwo
)
{
    // Make sure pNodeOne is not a descendent of pNodeTwo
    Gdrive_Cache_Node* pParent = pNodeOne->pParent;
    while (pParent != NULL)
    {
        assert(pParent != pNodeTwo && 
                "gdrive_cnode_swap(): pNodeOne is a descendent of pNodeTwo"
            );
        pParent = pParent->pParent;
    }
    
    Gdrive_Cache_Node* pTempParent = pNodeOne->pParent;
    Gdrive_Cache_Node* pTempLeft = pNodeOne->pLeft;
    Gdrive_Cache_Node* pTempRight = pNodeOne->pRight;
    
    if (pNodeOne->pLeft== pNodeTwo || pNodeOne->pRight == pNodeTwo)
    {
        // Node Two is a direct child of Node One
        
        pNodeOne->pLeft = pNodeTwo->pLeft;
        pNodeOne->pRight = pNodeTwo->pRight;
        
        if (pTempLeft == pNodeTwo)
        {
            pNodeTwo->pLeft = pNodeOne;
            pNodeTwo->pRight = pTempRight;
        }
        else
        {
            pNodeTwo->pLeft = pTempLeft;
            pNodeTwo->pRight = pNodeOne;
        }
        
        // Don't touch *ppFromParentTwo - it's either pNodeOne->pLeft 
        // or pNodeOne->pRight.
    }
    else
    {
        // Not direct parent/child
        
        pNodeOne->pParent = pNodeTwo->pParent;
        pNodeOne->pLeft = pNodeTwo->pLeft;
        pNodeOne->pRight = pNodeTwo->pRight;
        
        pNodeTwo->pLeft = pTempLeft;
        pNodeTwo->pRight = pTempRight;
        
        *ppFromParentTwo = pNodeOne;
    }
    
    pNodeTwo->pParent = pTempParent;
    *ppFromParentOne = pNodeTwo;
    
    // Fix the pParent pointers in each of the descendents. If pNodeTwo was
    // originally a direct child of pNodeOne, this also updates pNodeOne's
    // pParent.
    if (pNodeOne->pLeft)
        pNodeOne->pLeft->pParent = pNodeOne;
    if (pNodeOne->pRight)
        pNodeOne->pRight->pParent = pNodeOne;
    if (pNodeTwo->pLeft)
        pNodeTwo->pLeft->pParent = pNodeTwo;
    if (pNodeTwo->pRight)
        pNodeTwo->pRight->pParent = pNodeTwo; 
}

/*
 * NOT RECURSIVE.  FREES ONLY THE SINGLE NODE.
 */
static void
gdrive_cnode_free(Gdrive_Cache_Node* pNode)
{
    gdrive_finfo_cleanup(&(pNode->fileinfo));
    gdrive_fcontents_free_all(&(pNode->pContents));
    pNode->pContents = NULL;
    pNode->pLeft = NULL;
    pNode->pRight = NULL;
    free(pNode);
}

static Gdrive_File_Contents* gdrive_cnode_add_contents(Gdrive_Cache_Node* pNode)
{
    // Create the actual Gdrive_File_Contents struct, and add it to the existing
    // chain if there is one.
    Gdrive_File_Contents* pContents = gdrive_fcontents_add(pNode->pContents);
    if (pContents == NULL)
    {
        // Memory or file creation error
        return NULL;
    }
    
    // If there is no existing chain, point to the new struct as the start of a
    // new chain.
    if (pNode->pContents == NULL)
    {
        pNode->pContents = pContents;
    }
    
    
    return pContents;
}

static Gdrive_File_Contents* 
gdrive_cnode_create_chunk(Gdrive_Cache_Node* pNode, off_t offset, size_t size, 
                          bool fillChunk)
{
    // Get the normal chunk size for this file, the smallest multiple of
    // minChunkSize that results in maxChunks or fewer chunks. Avoid creating
    // a chunk of size 0 by forcing fileSize to be at least 1.
    size_t fileSize = (pNode->fileinfo.size > 0) ? pNode->fileinfo.size : 1;
    int maxChunks = gdrive_get_maxchunks();
    size_t minChunkSize = gdrive_get_minchunksize();

    size_t perfectChunkSize = _gdrive_divide_round_up(fileSize, maxChunks);
    size_t chunkSize = _gdrive_divide_round_up(perfectChunkSize, minChunkSize) *
            minChunkSize;
    
    // The actual chunk may be a multiple of chunkSize.  A read that starts at
    // "offset" and is "size" bytes long should be within this single chunk.
    off_t chunkStart = (offset / chunkSize) * chunkSize;
    off_t chunkOffset = offset % chunkSize;
    off_t endChunkOffset = chunkOffset + size - 1;
    size_t realChunkSize = _gdrive_divide_round_up(endChunkOffset, chunkSize) *
            chunkSize;
    
    Gdrive_File_Contents* pContents = gdrive_cnode_add_contents(pNode);
    if (pContents == NULL)
    {
        // Memory or file creation error
        return NULL;
    }
    
    if (fillChunk)
    {
        int success = gdrive_fcontents_fill_chunk(pContents,
                                                  pNode->fileinfo.id, 
                                                  chunkStart, realChunkSize
        );
        if (success != 0)
        {
            // Didn't write the file.  Clean up the new Gdrive_File_Contents 
            // struct
            gdrive_cnode_delete_file_contents(pNode, pContents);
            return NULL;
        }
    }
    // else we're not filling the chunk, do nothing
    
    //Success
    return pContents;
}

static size_t 
gdrive_file_read_next_chunk(Gdrive_File* pFile, char* destBuf, off_t offset, 
                            size_t size)
{
    // Gdrive_Filehandle and Gdrive_Cache_Node are the same thing, but it's 
    // easier to think of the filehandle as just a token used to refer to a 
    // file, whereas a cache node has internal structure to act upon.
    Gdrive_Cache_Node* pNode = pFile;
    
    // Do we already have a chunk that includes the starting point?
    Gdrive_File_Contents* pChunkContents = 
            gdrive_fcontents_find_chunk(pNode->pContents, offset);
    
    if (pChunkContents == NULL)
    {
        // Chunk doesn't exist, need to create and download it.
        pChunkContents = gdrive_cnode_create_chunk(pNode, offset, size, true);
        
        if (pChunkContents == NULL)
        {
            // Error creating the chunk
            // TODO: size_t is (or should be) unsigned. Rather than returning
            // a negative value for error, we should probably return 0 and add
            // a parameter for a pointer to an error value.
            return -EIO;
        }
    }
    
    // Actually read to the buffer and return the number of bytes read (which
    // may be less than size if we hit the end of the chunk), or return any 
    // error up to the caller.
    return gdrive_fcontents_read(pChunkContents, destBuf, offset, size);
}

static off_t 
gdrive_file_write_next_chunk(Gdrive_File* pFile, const char* buf, off_t offset, 
                             size_t size)
{
    // Gdrive_Filehandle and Gdrive_Cache_Node are the same thing, but it's 
    // easier to think of the filehandle as just a token used to refer to a 
    // file, whereas a cache node has internal structure to act upon.
    Gdrive_Cache_Node* pNode = pFile;
    
    // If the starting point is 1 byte past the end of the file, we'll extend 
    // the final chunk. Otherwise, we'll write to the end of the chunk and stop.
    bool extendChunk = (offset == (off_t) pNode->fileinfo.size);
    
    // Find the chunk that includes the starting point, or the last chunk if
    // the starting point is 1 byte past the end.
    off_t searchOffset = (extendChunk && offset > 0) ? offset - 1 : offset;
    Gdrive_File_Contents* pChunkContents = 
            gdrive_fcontents_find_chunk(pNode->pContents, searchOffset);
    
    if (pChunkContents == NULL)
    {
        // Chunk doesn't exist. This is an error unless the file size is 0.
        if (pNode->fileinfo.size == 0)
        {
            // File size is 0, and there is no existing chunk. Create one and 
            // try again.
            gdrive_cnode_create_chunk(pNode, 0, 1, false);
            pChunkContents = 
                    gdrive_fcontents_find_chunk(pNode->pContents, searchOffset);
        }
    }
    if (pChunkContents == NULL)
    {
        // Chunk still doesn't exist, return error.
        // TODO: size_t is (or should be) unsigned. Rather than returning
        // a negative value for error, we should probably return 0 and add
        // a parameter for a pointer to an error value.
        return -EINVAL;
    }
    
    // Actually write to the buffer and return the number of bytes read (which
    // may be less than size if we hit the end of the chunk), or return any 
    // error up to the caller.
    off_t bytesWritten = gdrive_fcontents_write(pChunkContents, 
                                                 buf, 
                                                 offset, 
                                                 size, 
                                                 extendChunk
            );
    
    if (bytesWritten > 0)
    {
        // Mark the file as having been written
        pNode->dirty = true;
        
        if ((size_t)(offset + bytesWritten) > pNode->fileinfo.size)
        {
            // Update the file size
            pNode->fileinfo.size = offset + bytesWritten;
        }
    }
    
    return bytesWritten;
}

static bool 
gdrive_file_check_perm(const Gdrive_Cache_Node* pNode, int accessFlags)
{
    // What permissions do we have?
    unsigned int perms = gdrive_finfo_real_perms(&(pNode->fileinfo));
    
    // What permissions do we need?
    unsigned int neededPerms = 0;
    // At least on my system, O_RDONLY is 0, which prevents testing for the
    // individual bit flag. On systems like mine, just assume we always need
    // read access. If there are other systems that have a different O_RDONLY
    // value, we'll test for the flag on those systems.
    if ((O_RDONLY == 0) || (accessFlags & O_RDONLY) || (accessFlags & O_RDWR))
    {
        neededPerms = neededPerms | S_IROTH;
    }
    if ((accessFlags & O_WRONLY) || (accessFlags & O_RDWR))
    {
        neededPerms = neededPerms | S_IWOTH;
    }
    
    // If there is anything we need but don't have, return false.
    return !(neededPerms & ~perms);
    
}

static size_t 
gdrive_file_uploadcallback(char* buffer, off_t offset, size_t size, 
                           void* userdata)
{
    // All we need to do is read from a Gdrive_File* file handle into a buffer.
    // We already know how to do exactly that.
    int returnVal = gdrive_file_read((Gdrive_File*) userdata, 
                                     buffer, 
                                     size, 
                                     offset
    );
    return (returnVal >= 0) ? (size_t) returnVal : (size_t)(-1);
}

static char* 
gdrive_file_sync_metadata_or_create(Gdrive_Fileinfo* pFileinfo, 
                                    const char* parentId, const char* filename,
                                    bool isFolder, int* pError)
{
    // For existing file, pFileinfo must be non-NULL. For creating new file,
    // both parentId and filename must be non-NULL.
    assert(pFileinfo || (parentId && filename));
    
    Gdrive_Fileinfo myFileinfo = {0};
    Gdrive_Fileinfo* pMyFileinfo;
    if (pFileinfo != NULL)
    {
        pMyFileinfo = pFileinfo;
        isFolder = (pMyFileinfo->type == GDRIVE_FILETYPE_FOLDER);
    }
    else
    {
        // We won't change anything, but need to cast away the const to make
        // this assignment.
        myFileinfo.filename = (char*) filename;
        myFileinfo.type = isFolder ? 
            GDRIVE_FILETYPE_FOLDER : GDRIVE_FILETYPE_FILE;
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        {
        myFileinfo.creationTime = ts;
        myFileinfo.accessTime = ts;
        myFileinfo.modificationTime = ts;
        }
        // else leave the times at 0 on failure
        
        pMyFileinfo = &myFileinfo;
    }
    
    
    // Set up the file resource as a JSON object
    Gdrive_Json_Object* uploadResourceJson = gdrive_json_new();
    if (uploadResourceJson == NULL)
    {
        *pError = ENOMEM;
        return NULL;
    }
    gdrive_json_add_string(uploadResourceJson, "title", pMyFileinfo->filename);
    if (pFileinfo == NULL)
    {
        // Only set parents when creating a new file
        Gdrive_Json_Object* parentsArray = 
                gdrive_json_add_new_array(uploadResourceJson, "parents");
        if (parentsArray == NULL)
        {
            *pError = ENOMEM;
            gdrive_json_kill(uploadResourceJson);
            return NULL;
        }
        Gdrive_Json_Object* parentIdObj = gdrive_json_new();
        gdrive_json_add_string(parentIdObj, "id", parentId);
        gdrive_json_array_append_object(parentsArray, parentIdObj);
    }
    if (isFolder)
    {
        gdrive_json_add_string(uploadResourceJson, "mimeType", 
                               "application/vnd.google-apps.folder"
                );
    }
    char* timeString = malloc(GDRIVE_TIMESTRING_LENGTH);
    if (timeString == NULL)
    {
        // Memory error
        gdrive_json_kill(uploadResourceJson);
        *pError = ENOMEM;
        return NULL;
    }
    // Reuse the same timeString for atime and mtime. Can't change ctime.
    if (gdrive_finfo_get_atime_string(pMyFileinfo, timeString, 
                                      GDRIVE_TIMESTRING_LENGTH) 
            != 0)
    {
        gdrive_json_add_string(uploadResourceJson, 
                               "lastViewedByMeDate", 
                               timeString
                );
    }
    bool hasMtime = false;
    if (gdrive_finfo_get_mtime_string(pMyFileinfo, timeString, 
                                      GDRIVE_TIMESTRING_LENGTH) 
            != 0)
    {
        gdrive_json_add_string(uploadResourceJson, "modifiedDate", timeString);
        hasMtime = true;
    }
    free(timeString);
    timeString = NULL;
    
    // Convert the JSON into a string
    char* uploadResourceStr = 
        gdrive_json_to_new_string(uploadResourceJson, false);
    gdrive_json_kill(uploadResourceJson);
    if (uploadResourceStr == NULL)
    {
        *pError = ENOMEM;
        return NULL;
    }
    
    
    // Full URL has '/' and the file ID appended for an existing file, or just
    // the base URL for a new one.
    char* url;
    if (pFileinfo == NULL)
    {
        // New file, just need the base URL
        size_t urlSize = strlen(GDRIVE_URL_FILES) + 1;
        url = malloc(urlSize);
        if (url == NULL)
        {
            *pError = ENOMEM;
            return NULL;
        }
        strncpy(url, GDRIVE_URL_FILES, urlSize);
    }
    else
    {
        // Existing file, need base URL + '/' + file ID
        size_t baseUrlLength = strlen(GDRIVE_URL_FILES);
        size_t fileIdLength = strlen(pMyFileinfo->id);
        url = malloc(baseUrlLength + fileIdLength + 2);
        if (url == NULL)
        {
            *pError = ENOMEM;
            return NULL;
        }
        strncpy(url, GDRIVE_URL_FILES, baseUrlLength);
        url[baseUrlLength] = '/';
        strncpy(url + baseUrlLength + 1, pMyFileinfo->id, fileIdLength + 1);
    }
    
    // Set up the network request
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        *pError = ENOMEM;
        gdrive_xfer_free(pTransfer);
        free(uploadResourceStr);
        return NULL;
    }
    // URL, header, and updateViewedDate query parameter always get added. The 
    // setModifiedDate query parameter only gets set when hasMtime is true. Any 
    // of these can fail with an out of memory error (returning non-zero).
    if ((gdrive_xfer_set_url(pTransfer, url) ||
            gdrive_xfer_add_header(pTransfer, "Content-Type: application/json"))
            ||
            (hasMtime && 
            gdrive_xfer_add_query(pTransfer, "setModifiedDate", "true")) ||
            gdrive_xfer_add_query(pTransfer, "updateViewedDate", "false")
        )
    {
        *pError = ENOMEM;
        gdrive_xfer_free(pTransfer);
        free(uploadResourceStr);
        free(url);
        return NULL;
    }
    free(url);
    gdrive_xfer_set_requesttype(pTransfer, (pFileinfo != NULL) ? 
        GDRIVE_REQUEST_PATCH : GDRIVE_REQUEST_POST);
    gdrive_xfer_set_body(pTransfer, uploadResourceStr);
    
    // Do the transfer
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    free(uploadResourceStr);
    
    if (pBuf == NULL || gdrive_dlbuf_get_httpResp(pBuf) >= 400)
    {
        // Transfer was unsuccessful
        *pError = EIO;
        gdrive_dlbuf_free(pBuf);
        return NULL;
    }
    
    // Extract the file ID from the returned resource
    Gdrive_Json_Object* pObj = 
            gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
    gdrive_dlbuf_free(pBuf);
    if (pObj == NULL)
    {
        // Either memory error, or couldn't convert the response to JSON.
        // More likely memory.
        *pError = ENOMEM;
        return NULL;
    }
    char* fileId = gdrive_json_get_new_string(pObj, "id", NULL);
    gdrive_json_kill(pObj);
    if (fileId == NULL)
    {
        // Either memory error, or couldn't extract the desired string, can't
        // tell which.
        *pError = EIO;
        return NULL;
    }
    
    pMyFileinfo->dirtyMetainfo = false;
    return fileId;
}



