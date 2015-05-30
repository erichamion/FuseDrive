


#include "gdrive-transfer.h"
#include "gdrive-query.h"
#include "gdrive-info.h"

#include <string.h>


#define GDRIVE_RETRY_LIMIT 5


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/


typedef struct Gdrive_Transfer 
{
    enum Gdrive_Request_Type requestType;
    bool retryOnAuthError;
    char* url;
    Gdrive_Query* pQuery;
    Gdrive_Query* pPostData;
    const char* body;
    struct curl_slist* pHeaders;
    FILE* destFile;
    gdrive_xfer_upload_callback uploadCallback;
    void* userdata;
    off_t uploadOffset;
} Gdrive_Transfer;


/*
 * Returns 0 on success, other on failure.
 */
static int 
gdrive_xfer_add_query_or_post(Gdrive_Query** ppQuery, const char* field, 
                              const char* value);

static size_t 
gdrive_xfer_upload_callback_internal(char* buffer, size_t size, size_t nitems, 
                                     void* instream);


/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors and destructors
 ******************/

Gdrive_Transfer* gdrive_xfer_create()
{
    Gdrive_Transfer* returnVal = malloc(sizeof(Gdrive_Transfer));
    if (returnVal != NULL)
    {
        memset(returnVal, 0, sizeof(Gdrive_Transfer));
        returnVal->retryOnAuthError = true;
        returnVal->pHeaders = gdrive_get_authbearer_header(NULL);
        if (returnVal->pHeaders == NULL)
        {
            // Error. Cleanup and return NULL.
            free(returnVal);
            return NULL;
        }
    }
    
    return returnVal;
}

void gdrive_xfer_free(Gdrive_Transfer* pTransfer)
{
    if (pTransfer == NULL)
    {
        // Nothing to do
        return;
    }
    
    // If these need freed, they'll be non-NULL. If they don't need freed,
    // they'll be NULL, and it's safe to free them anyway.
    free(pTransfer->url);
    pTransfer->url = NULL;
    gdrive_query_free(pTransfer->pQuery);
    pTransfer->pQuery = NULL;
    gdrive_query_free(pTransfer->pPostData);
    pTransfer->pPostData = NULL;
    if (pTransfer->pHeaders != NULL)
    {
        curl_slist_free_all(pTransfer->pHeaders);
        pTransfer->pHeaders = NULL;
    }
    
    // Free the overall struct
    free(pTransfer);
}



/******************
 * Getter and setter functions
 ******************/

void gdrive_xfer_set_requesttype(Gdrive_Transfer* pTransfer, 
                                 enum Gdrive_Request_Type requestType
)
{
    pTransfer->requestType = requestType;
}

void gdrive_xfer_set_retryonautherror(Gdrive_Transfer* pTransfer, bool retry)
{
    pTransfer->retryOnAuthError = retry;
}

int gdrive_xfer_set_url(Gdrive_Transfer* pTransfer, const char* url)
{
    size_t size = strlen(url) + 1;
    pTransfer->url = malloc(size);
    if (pTransfer->url == NULL)
    {
        // Memory error
        return -1;
    }
    memcpy(pTransfer->url, url, size);
    return 0;
}

void gdrive_xfer_set_destfile(Gdrive_Transfer* pTransfer, FILE* destFile)
{
    pTransfer->destFile = destFile;
}

void gdrive_xfer_set_body(Gdrive_Transfer* pTransfer, const char* body)
{
    pTransfer->body = body;
}

void gdrive_xfer_set_uploadcallback(Gdrive_Transfer* pTransfer, 
                                    gdrive_xfer_upload_callback callback,
                                    void* userdata
)
{
    pTransfer->userdata = userdata;
    pTransfer->uploadOffset = 0;
    pTransfer->uploadCallback = callback;
}



/******************
 * Other accessible functions
 ******************/

int gdrive_xfer_add_query(Gdrive_Transfer* pTransfer, 
                          const char* field, 
                          const char* value
)
{
    return gdrive_xfer_add_query_or_post(&(pTransfer->pQuery), field, value);
}

int gdrive_xfer_add_postfield(Gdrive_Transfer* pTransfer, 
                          const char* field, 
                          const char* value
)
{
    return gdrive_xfer_add_query_or_post(&(pTransfer->pPostData), field, value);
}

int gdrive_xfer_add_header(Gdrive_Transfer* pTransfer, const char* header)
{
    pTransfer->pHeaders = curl_slist_append(pTransfer->pHeaders, header);
    return (pTransfer->pHeaders == NULL);
}

Gdrive_Download_Buffer* gdrive_xfer_execute(Gdrive_Transfer* pTransfer)
{
    if (pTransfer->url == NULL)
    {
        // Invalid parameter, need at least a URL.
        return NULL;
    }
    
    CURL* curlHandle = gdrive_get_curlhandle();
    
    // Set the request type
    switch (pTransfer->requestType)
    {
    case GDRIVE_REQUEST_GET:
        curl_easy_setopt(curlHandle, CURLOPT_HTTPGET, 1);
        break;
        
    case GDRIVE_REQUEST_POST:
        curl_easy_setopt(curlHandle, CURLOPT_POST, 1);
        break;
        
    case GDRIVE_REQUEST_PUT:
        curl_easy_setopt(curlHandle, CURLOPT_UPLOAD, 1);
        break;
        
    case GDRIVE_REQUEST_PATCH:
        curl_easy_setopt(curlHandle, CURLOPT_POST, 1);
        curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, "PATCH");
        break;
        
    default:
        // Unsupported request type.  
        curl_easy_cleanup(curlHandle);
        return NULL;
    }
    
    // Append any query parameters to the URL, and add the full URL to the
    // curl handle.
    char* fullUrl = NULL;
    fullUrl = gdrive_query_assemble(pTransfer->pQuery, pTransfer->url);
    if (fullUrl == NULL)
    {
        // Memory error or invalid URL
        curl_easy_cleanup(curlHandle);
        return NULL;
    }
    curl_easy_setopt(curlHandle, CURLOPT_URL, fullUrl);
    free(fullUrl);
    fullUrl = NULL;
    
    // Set simple POST fields, if applicable
    if (pTransfer->body != NULL)
    {
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE, -1L);
        curl_easy_setopt(curlHandle, CURLOPT_COPYPOSTFIELDS, pTransfer->body);
    }
    else if (pTransfer->pPostData != NULL)
    {
        char* postData = gdrive_query_assemble(pTransfer->pPostData, NULL);
        if (postData == NULL)
        {
            // Memory error or invalid query
            curl_easy_cleanup(curlHandle);
            return NULL;
        }
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE, -1L);
        curl_easy_setopt(curlHandle, CURLOPT_COPYPOSTFIELDS, postData);
        free(postData);
    }
    
    // Set upload data callback, if applicable
    if (pTransfer->uploadCallback != NULL)
    {
        gdrive_xfer_add_header(pTransfer, "Transfer-Encoding: chunked");
        curl_easy_setopt(curlHandle, 
                         CURLOPT_READFUNCTION, 
                         gdrive_xfer_upload_callback_internal
                );
        curl_easy_setopt(curlHandle, CURLOPT_READDATA, pTransfer);
    }
    

    
    // Set headers
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, pTransfer->pHeaders);
    
    
    Gdrive_Download_Buffer* pBuf;
    pBuf = gdrive_dlbuf_create((pTransfer->destFile == NULL) ? 512 : 0, 
                               pTransfer->destFile
            );
    if (pBuf == NULL)
    {
        // Memory error.
        curl_easy_cleanup(curlHandle);
        return NULL;
    }
    
    int result = gdrive_dlbuf_download_with_retry(pBuf, curlHandle, 
                                                  pTransfer->retryOnAuthError, 
                                                  0, GDRIVE_RETRY_LIMIT
    );
    curl_easy_cleanup(curlHandle);
    
    if (result != 0)
    {
        // Download failure
        gdrive_dlbuf_free(pBuf);
        return NULL;
    }
    
    // The HTTP Response may be success (e.g., 200) or failure (400 or higher),
    // but the actual request succeeded as far as libcurl is concerned.  Return
    // the buffer.
    return pBuf;
}


/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

static int gdrive_xfer_add_query_or_post(Gdrive_Query** ppQuery, 
                                         const char* field, 
                                         const char* value
)
{
    *ppQuery = gdrive_query_add(*ppQuery, field, value);
    return (*ppQuery == NULL);
}

static size_t 
gdrive_xfer_upload_callback_internal(char* buffer, size_t size, size_t nitems, 
                                     void* instream)
{
    // Get the transfer struct.
    Gdrive_Transfer* pTransfer = (Gdrive_Transfer*) instream;
    size_t bytesTransferred = 
            pTransfer->uploadCallback(buffer, pTransfer->uploadOffset, 
                                      size * nitems, pTransfer->userdata
            );
    if (bytesTransferred == (size_t)(-1))
    {
        // Upload error
        return CURL_READFUNC_ABORT;
    }
    // else succeeded
    
    pTransfer->uploadOffset += bytesTransferred;
    return bytesTransferred;
}
