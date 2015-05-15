
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
    // Construct the base URL in the form of "<GDRIVE_URL_FILES>/<fileId>".
    char* fileUrl = malloc(strlen(GDRIVE_URL_FILES) + 
                           strlen(fileId) + 2
    );
    if (fileUrl == NULL)
    {
        // Memory error
        return -1;
    }
    strcpy(fileUrl, GDRIVE_URL_FILES);
    strcat(fileUrl, "/");
    strcat(fileUrl, fileId);
    
    // Construct query parameters
    Gdrive_Query* pQuery = NULL;
    pQuery = gdrive_query_add(pQuery, "updateViewedDate", "false");
    pQuery = gdrive_query_add(pQuery, "alt", "media");
    if (pQuery == NULL)
    {
        // Memory error
        free(fileUrl);
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
        gdrive_query_free(pQuery);
        free(fileUrl);
        return -1;
    }
    snprintf(rangeHeader, rangeSize, "Range: bytes=%ld-%ld", start, end);
    struct curl_slist* pHeaders = curl_slist_append(NULL, rangeHeader);
    free(rangeHeader);
    if (pHeaders == NULL)
    {
        // An error occurred
        gdrive_query_free(pQuery);
        free(fileUrl);
        return -1;
    }
    
    // Make sure the file position is at the start and any stream errors are
    // cleared (this should be redundant, since we should normally have a newly
    // created and opened temporary file).
    rewind(pContents->fh);
    
    // Perform the transfer
    Gdrive_Download_Buffer* pBuf = gdrive_do_transfer(GDRIVE_REQUEST_GET, true,
                                                      fileUrl, pQuery, 
                                                      pHeaders, pContents->fh
    );
    
    bool success = (pBuf != NULL && 
            gdrive_dlbuf_get_httpResp(pBuf) < 400
            );
    gdrive_dlbuf_free(pBuf);
    gdrive_query_free(pQuery);
    free(fileUrl);
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
