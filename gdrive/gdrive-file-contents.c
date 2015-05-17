
#include "gdrive-file-contents.h"

#include <string.h>




/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_File_Contents
{
    off_t start;
    off_t end;
    FILE* fh;
    struct Gdrive_File_Contents* pNext;
} Gdrive_File_Contents;

static Gdrive_File_Contents* gdrive_fcontents_create();



/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors and destructors
 ******************/

Gdrive_File_Contents* gdrive_fcontents_add(Gdrive_File_Contents* pHead)
{
    // Create the actual file contents struct.
    Gdrive_File_Contents* pNew = gdrive_fcontents_create();
    
    // Find the last entry in the file contents list, and add the new one to
    // the end.
    Gdrive_File_Contents* pContents = pHead;
    if (pHead != NULL)
    {
        while (pContents->pNext != NULL)
        {
            pContents = pContents->pNext;
        }
        pContents->pNext = pNew;
    }
    // else we weren't given a list to append to, do nothing.
    
    return pNew;
}

void gdrive_fcontents_delete(Gdrive_File_Contents* pContents, 
                             Gdrive_File_Contents** ppHead
)
{
    // Find the pointer leading to pContents.
    Gdrive_File_Contents** ppContents = ppHead;
    while (*ppContents != NULL && *ppContents != pContents)
    {
        ppContents = &((*ppContents)->pNext);
    }
    
    // Take pContents out of the chain
    if (*ppContents != NULL)
    {
        *ppContents = pContents->pNext;
    }
    
    // Close the temp file
    if (pContents->fh != NULL)
    {
        fclose(pContents->fh);
        pContents->fh = NULL;
    }
    
    free(pContents);
}


void gdrive_fcontents_free_all(Gdrive_File_Contents** ppContents)
{
    if (ppContents == NULL || *ppContents == NULL)
    {
        // Nothing to do
        return;
    }
    
    // Convenience assignment
    Gdrive_File_Contents* pContents = *ppContents;
    
    // Free the rest of the list after the current item.
    gdrive_fcontents_free_all(&(pContents->pNext));
    
    // Close the temp file, which will automatically delete it.
    if (pContents->fh != NULL)
    {
        fclose(pContents->fh);
        pContents->fh = NULL;
    }
    
    // Free the memory associated with the item
    free(pContents);
    
    // Clear the pointer to the item
    *ppContents = NULL;
}


/******************
 * Getter and setter functions
 ******************/

// No getter or setter functions


/******************
 * Other accessible functions
 ******************/

Gdrive_File_Contents* 
gdrive_fcontents_find_chunk(Gdrive_File_Contents* pHead, off_t offset)
{
    if (pHead == NULL || pHead->fh == NULL)
    {
        // Nothing here, return failure.
        return NULL;
    }
    
    if (offset > pHead->start && offset < pHead->end)
    {
        // Found it!
        return pHead;
    }
    
    // It's not at this node.  Try the next one.
    return gdrive_fcontents_find_chunk(pHead->pNext, offset);
}

int gdrive_fcontents_fill_chunk(Gdrive_File_Contents* pContents, 
                                const char* fileId,
                                off_t start,
                                size_t size
)
{
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        // Memory error
        return -1;
    }
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_GET);
    
    // Construct the base URL in the form of "<GDRIVE_URL_FILES>/<fileId>".
    char* fileUrl = malloc(strlen(GDRIVE_URL_FILES) + 
                           strlen(fileId) + 2
    );
    if (fileUrl == NULL)
    {
        // Memory error
        gdrive_xfer_free(pTransfer);
        return -1;
    }
    strcpy(fileUrl, GDRIVE_URL_FILES);
    strcat(fileUrl, "/");
    strcat(fileUrl, fileId);
    if (gdrive_xfer_set_url(pTransfer, fileUrl) != 0)
    {
        // Error
        free(fileUrl);
        gdrive_xfer_free(pTransfer);
        return -1;
    }
    free(fileUrl);
    
    // Construct query parameters
    if (
            gdrive_xfer_add_query(pTransfer, "updateViewedDate", "false") ||
            gdrive_xfer_add_query(pTransfer, "alt", "media")
        )
    {
        // Error
        gdrive_xfer_free(pTransfer);
        return -1;
    }
    
    // Add the Range header.  Per 
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.35 it is
    // fine for the end of the range to be past the end of the file, so we won't
    // worry about the file size.
    off_t end = start + size - 1;
    int rangeSize = snprintf(NULL, 0, "Range: bytes=%ld-%ld", start, end) + 1;
    char* rangeHeader = malloc(rangeSize);
    if (rangeHeader == NULL)
    {
        // Memory error
        gdrive_xfer_free(pTransfer);
        return -1;
    }
    snprintf(rangeHeader, rangeSize, "Range: bytes=%ld-%ld", start, end);
    if (gdrive_xfer_add_header(pTransfer, rangeHeader) != 0)
    {
        // Error
        free(rangeHeader);
        gdrive_xfer_free(pTransfer);
        return -1;
    }
    free(rangeHeader);
    
    // Set the destination file to the current chunk's handle
    gdrive_xfer_set_destfile(pTransfer, pContents->fh);
    
    // Make sure the file position is at the start and any stream errors are
    // cleared (this should be redundant, since we should normally have a newly
    // created and opened temporary file).
    rewind(pContents->fh);
    
    // Perform the transfer
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    bool success = (pBuf != NULL && gdrive_dlbuf_get_httpResp(pBuf) < 400);
    gdrive_dlbuf_free(pBuf);
    if (success)
    {
        pContents->start = start;
        pContents->end = start + size - 1;
        return 0;
    }
    //else failed
    return -1;
}

size_t gdrive_fcontents_read(Gdrive_File_Contents* pContents, 
                             char* destBuf, 
                             off_t offset, 
                             size_t size
)
{
    // If given a NULL buffer pointer, just return the number of bytes that 
    // would have been read upon success.
    if (destBuf == NULL)
    {
        size_t maxSize = pContents->end - offset + 1;
        return (size > maxSize) ? size : maxSize;
    }
    
    // Read the data into the supplied buffer.
    FILE* chunkFile = pContents->fh;
    fseek(chunkFile, offset - pContents->start, SEEK_SET);
    size_t bytesRead = fread(destBuf, 1, size, chunkFile);
    
    // If an error occurred, return negative.
    if (bytesRead < size)
    {
        int err = ferror(chunkFile);
        if (err != 0)
        {
            rewind(chunkFile);
            return -err;
        }
    }
    
    // Return the number of bytes read (which may be less than size if we hit
    // EOF).
    return bytesRead;
}
    
off_t gdrive_fcontents_write(Gdrive_File_Contents* pContents, 
                             const char* buf, 
                             off_t offset,
                             size_t size,
                             bool extendChunk
)
{
    // Only write to the end of the chunk, unless extendChunk is true
    size_t maxSize = pContents->end - offset;
    size_t realSize = (extendChunk || size <= maxSize) ? size : maxSize;
    
    // Write the data from the supplied buffer.
    FILE* chunkFile = pContents->fh;
    fseek(chunkFile, offset - pContents->start, SEEK_SET);
    size_t bytesWritten = fwrite(buf, 1, size, chunkFile);
    
    // Extend the chunk's ending offset if needed
    if ((off_t) (offset + bytesWritten - 1) > pContents->end)
    {
        pContents->end = offset + bytesWritten - 1;
    }
    
    // If an error occurred, return negative.
    if (bytesWritten < realSize)
    {
        int err = ferror(chunkFile);
        if (err != 0)
        {
            rewind(chunkFile);
            return -err;
        }
    }
    
    // Return the number of bytes read (which may be less than size if we hit
    // end of chunk).
    return bytesWritten;
}



/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

static Gdrive_File_Contents* gdrive_fcontents_create()
{
    Gdrive_File_Contents* pContents = malloc(sizeof(Gdrive_File_Contents));
    if (pContents == NULL)
    {
        // Memory error
        return NULL;
    }
    memset(pContents, 0, sizeof(Gdrive_File_Contents));
        
    // Create a temporary file on disk.  This will automatically be deleted
    // when the file is closed or when this program terminates, so no 
    // cleanup is needed.
    pContents->fh = tmpfile();
    if (pContents->fh == NULL)
    {
        // File creation error
        free(pContents);
        return NULL;
    }
    
    return pContents;
}
