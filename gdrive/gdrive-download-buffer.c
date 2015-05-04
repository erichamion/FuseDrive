

#include <stdlib.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <string.h>


#include "gdrive-download-buffer.h"


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_Download_Buffer
{
    size_t allocatedSize;
    size_t usedSize;
    long httpResp;
    CURLcode resultCode;
    char* data;
    FILE* fh;
} Gdrive_Download_Buffer;

static size_t 
gdrive_dlbuf_callback(char *newData, size_t size, size_t nmemb, void *userdata);




/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

Gdrive_Download_Buffer* gdrive_dlbuf_create(size_t initialSize, FILE* fh)
{
    Gdrive_Download_Buffer* pBuf = malloc(sizeof(Gdrive_Download_Buffer));
    if (pBuf == NULL)
    {
        // Couldn't allocate memory for the struct.
        return NULL;
    }
    pBuf->usedSize = 0;
    pBuf->allocatedSize = initialSize;
    pBuf->httpResp = 0;
    pBuf->resultCode = 0;
    pBuf->data = NULL;
    pBuf->fh = fh;
    if (initialSize != 0)
    {
        if ((pBuf->data = malloc(initialSize)) == NULL)
        {
            // Couldn't allocate the requested memory for the data.
            // Free the struct's memory and return NULL.
            free(pBuf);
            return NULL;
        }
    }
    return pBuf;
}

void gdrive_dlbuf_free(Gdrive_Download_Buffer* pBuf)
{
    if (pBuf != NULL && pBuf->data != NULL && pBuf->allocatedSize > 0)
    {
        free(pBuf->data);
        pBuf->data = NULL;
    }
    free(pBuf);
}

long gdrive_dlbuf_get_httpResp(Gdrive_Download_Buffer* pBuf)
{
    return pBuf->httpResp;
}

const char* gdrive_dlbuf_get_data(Gdrive_Download_Buffer* pBuf)
{
    return pBuf->data;
}

CURLcode gdrive_dlbuf_download(Gdrive_Download_Buffer* pBuf, CURL* curlHandle)
{
    // Make sure data gets written at the start of the buffer.
    pBuf->usedSize = 0;
    
    // Accept compressed responses.
    curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, "");
    
    // Automatically follow redirects
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1);

    
    // Set the destination - either our own callback function to fill the
    // in-memory buffer, or the default libcurl function to write to a FILE*.
    if (pBuf->fh == NULL)
    {
        curl_easy_setopt(curlHandle, 
                         CURLOPT_WRITEFUNCTION, 
                         gdrive_dlbuf_callback
                );
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, pBuf);
    }
    else
    {
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, pBuf->fh);
    }
    
    // Do the transfer.
    pBuf->resultCode = curl_easy_perform(curlHandle);
    
    // Get the HTTP response
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &(pBuf->httpResp));
    
    return pBuf->resultCode;
}



/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

static size_t 
gdrive_dlbuf_callback(char *newData, size_t size, size_t nmemb, void *userdata)
{
    if (size == 0 || nmemb == 0)
    {
        // No data
        return 0;
    }
    
    Gdrive_Download_Buffer* pBuffer = (Gdrive_Download_Buffer*) userdata;
    
    // Find the length of the data, and allocate more memory if needed.  If
    // textMode is true, include an extra byte to explicitly null terminate.
    // If downloading text data that's already null terminated, the extra NULL
    // doesn't hurt anything. If downloading binary data, the NULL is past the
    // end of the data and still doesn't hurt anything.
    size_t dataSize = size * nmemb;
    size_t totalSize = dataSize + pBuffer->usedSize + 1;
    if (totalSize > pBuffer->allocatedSize)
    {
        // Allow extra room to reduce the number of realloc's.
        size_t minSize = totalSize + dataSize;
        size_t doubleSize = 2 * pBuffer->allocatedSize;
        size_t allocSize = (minSize > doubleSize) ? minSize : doubleSize;
        pBuffer->data = realloc(pBuffer->data, allocSize);
        if (pBuffer->data == NULL)
        {
            // Memory allocation error.
            pBuffer->allocatedSize = 0;
            return 0;
        }
        pBuffer->allocatedSize = allocSize;
    }
    
    // Copy the data
    memcpy(pBuffer->data + pBuffer->usedSize, newData, dataSize);
    pBuffer->usedSize += dataSize;
    
    // Add the null terminator
    pBuffer->data[totalSize - 1] = '\0';
    
    return dataSize;
}
