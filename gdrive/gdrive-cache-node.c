



#include "gdrive-cache-node.h"
#include "gdrive-cache.h"

#include <errno.h>
#include <string.h>
#include <fcntl.h>


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_Cache_Node
{
    time_t lastUpdateTime;
    int openCount;
    int openWrites;
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
gdrive_cnode_create_chunk(Gdrive_Cache_Node* pNode, off_t offset, size_t size);

static size_t 
gdrive_file_read_next_chunk(Gdrive_File* pNode, char* destBuf, off_t offset,
                            size_t size);

static size_t 
gdrive_file_write_next_chunk(Gdrive_File* pFile, const char* buf, off_t offset, 
                             size_t size);

static bool 
gdrive_file_check_perm(const Gdrive_Cache_Node* pNode, int accessFlags);




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
        if (*ppNode != NULL)
        {
            // Convenience to avoid things like "return &((*ppNode)->fileinfo);"
            Gdrive_Cache_Node* pNode = *ppNode;
            
            // Copy the fileId into the fileinfo. Everything else is left null.
            pNode->fileinfo.id = malloc(strlen(fileId) + 1);
            if (pNode->fileinfo.id == NULL)
            {
                // Memory error.
                gdrive_cnode_free(pNode);
                *ppNode = NULL;
                return NULL;
            }
            strcpy(pNode->fileinfo.id, fileId);
            
            // Since this is a new entry, set the node's updated time.
            pNode->lastUpdateTime = time(NULL);
            
            return pNode;
        }
    }
    
    // Convenience to avoid things like "&((*ppNode)->pRight)"
    Gdrive_Cache_Node* pNode = *ppNode;
    
    // Root node exists, try to find the fileId in the tree.
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
    
//    // Find the pointer from pNode's new parent.  We don't need to worry about
//    // a NULL pParent, since pNode can't be at the root of the tree after
//    // swapping.
//    Gdrive_Cache_Node** ppFromNewParent = (pNode->pParent->pLeft == pNode ?
//        &(pNode->pParent->pLeft) :
//        &(pNode->pParent->pRight)
//            );
    
    // Now delete the node from its new position.
    gdrive_cnode_delete(pNode, ppToRoot);
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


/*************************************************************************
 * Public functions to support Gdrive_File usage
 *************************************************************************/

Gdrive_File* gdrive_file_open(const char* fileId, int flags, int* pError)
{
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
        *pError = EPERM;
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
    // Gdrive_Filehandle and Gdrive_Cache_Node are the same thing, but it's 
    // easier to think of the filehandle as just a token used to refer to a 
    // file, whereas a cache node has internal structure to act upon.
    Gdrive_Cache_Node* pNode = pFile;
    
    // Decrement open file counts.
    pNode->openCount--;
    
    if ((flags & O_WRONLY) || (flags & O_RDWR))
    {
        // Was opened for writing
        pNode->openWrites--;
        
        // TODO: Upload the new version of the file to the Google Drive servers
    }
    
    
    // Get rid of any downloaded temp files if they aren't needed.
    // TODO: Consider keeping some closed files around in case they're reopened
    if (pNode->openCount == 0)
    {
        gdrive_fcontents_free_all(&(pNode->pContents));
    }
}

int gdrive_file_read(Gdrive_File* fh, char* buf, size_t size, off_t offset)
{
    // Make sure we have at least read access for the file.
    if (!gdrive_file_check_perm(fh, O_RDONLY))
    {
        // Access error
        return -EACCES;
    }
    
    off_t nextOffset = offset;
    off_t bufferOffset = 0;
    size_t bytesRemaining = size;
    
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
            return size - bytesRemaining;
        }
        nextOffset += bytesRead;
        bufferOffset += bytesRead;
        bytesRemaining -= bytesRead;
    }
    
    return size;
}

int gdrive_file_write(Gdrive_File* fh, 
                      const char* buf, 
                      size_t size, 
                      off_t offset
)
{
    // Make sure we have read and write access for the file.
    if (!gdrive_file_check_perm(fh, O_RDWR))
    {
        // Access error
        return -EACCES;
    }
    
    // Read any needed chunks into the cache.
    gdrive_file_read(fh, NULL, size, offset);
    
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

const Gdrive_Fileinfo* gdrive_file_get_info(Gdrive_File* fh)
{
    if (fh == NULL)
    {
        // Invalid argument
        return NULL;
    }
    
    // Gdrive_Filehandle and Gdrive_Cache_Node are typedefs of the same struct,
    // but it's easier to think about them differently. A filehandle is a token,
    // and a cache node has an internal structure.
    Gdrive_Cache_Node* pNode = fh;
    return gdrive_cnode_get_fileinfo(pNode);
}

int gdrive_file_get_perms(const Gdrive_File* fh)
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

static void
gdrive_cnode_swap(Gdrive_Cache_Node** ppFromParentOne,
                        Gdrive_Cache_Node* pNodeOne,
                        Gdrive_Cache_Node** ppFromParentTwo,
                        Gdrive_Cache_Node* pNodeTwo
)
{
    // Swap the pointers from the parents
    *ppFromParentOne = pNodeTwo;
    *ppFromParentTwo = pNodeOne;
    
    Gdrive_Cache_Node* pTempParent = pNodeOne->pParent;
    Gdrive_Cache_Node* pTempLeft = pNodeOne->pLeft;
    Gdrive_Cache_Node* pTempRight = pNodeOne->pRight;
    
    pNodeOne->pParent = pNodeTwo->pParent;
    pNodeOne->pLeft = pNodeTwo->pLeft;
    pNodeOne->pRight = pNodeTwo->pRight;
    
    pNodeTwo->pParent = pTempParent;
    pNodeTwo->pLeft = pTempLeft;
    pNodeTwo->pRight = pTempRight;
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
gdrive_cnode_create_chunk(Gdrive_Cache_Node* pNode, off_t offset, size_t size)
{
    // Get the normal chunk size for this file, the smallest multiple of
    // minChunkSize that results in maxChunks or fewer chunks.
    size_t fileSize = pNode->fileinfo.size;
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
    
    int success = gdrive_fcontents_fill_chunk(pContents,
                                              pNode->fileinfo.id, 
                                              chunkStart, realChunkSize
    );
    if (success != 0)
    {
        // Didn't write the file.  Clean up the new Gdrive_File_Contents struct
        gdrive_cnode_delete_file_contents(pNode, pContents);
        return NULL;
    }
    
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
        pChunkContents = gdrive_cnode_create_chunk(pNode, offset, size);
        
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

static size_t 
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
    off_t searchOffset = (extendChunk) ? offset - 1 : offset;
    Gdrive_File_Contents* pChunkContents = 
            gdrive_fcontents_find_chunk(pNode->pContents, searchOffset);
    
    if (pChunkContents == NULL)
    {
        // Chunk doesn't exist, return error.
        // TODO: size_t is (or should be) unsigned. Rather than returning
        // a negative value for error, we should probably return 0 and add
        // a parameter for a pointer to an error value.
        return -EINVAL;
    }
    
    // Actually write to the buffer and return the number of bytes read (which
    // may be less than size if we hit the end of the chunk), or return any 
    // error up to the caller.
    size_t bytesWritten = gdrive_fcontents_write(pChunkContents, 
                                                 buf, 
                                                 offset, 
                                                 size, 
                                                 extendChunk
            );
    // Update the size of the file if applicable
    if (offset + bytesWritten > pNode->fileinfo.size)
    {
        pNode->fileinfo.size = offset + bytesWritten;
    }
    return bytesWritten;
}

static bool 
gdrive_file_check_perm(const Gdrive_Cache_Node* pNode, int accessFlags)
{
    // What permissions do we have?
    int perms = gdrive_finfo_real_perms(&(pNode->fileinfo));
    
    // What permissions do we need?
    int neededPerms = 0;
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