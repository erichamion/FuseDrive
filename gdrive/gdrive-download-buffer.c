#include "gdrive-download-buffer.h"
#include "gdrive-info.h"

#include <string.h>



/*************************************************************************
 * Constants needed only internally within this file
 *************************************************************************/

#define GDRIVE_403_RATELIMIT "rateLimitExceeded"
#define GDRIVE_403_USERRATELIMIT "userRateLimitExceeded"


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
    char* pReturnedHeaders;
    size_t returnedHeaderSize;
    FILE* fh;
} Gdrive_Download_Buffer;

static size_t 
gdrive_dlbuf_callback(char *newData, size_t size, size_t nmemb, void *userdata);

static size_t
gdrive_dlbuf_header_callback(char* buffer, size_t size, size_t nitems, 
                             void* userdata);

static enum Gdrive_Retry_Method 
gdrive_dlbuf_retry_on_error(Gdrive_Download_Buffer* pBuf, long httpResp);

static void gdrive_exponential_wait(int tryNum);







/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors and destructors
 ******************/

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
    pBuf->pReturnedHeaders = malloc(1);
    pBuf->pReturnedHeaders[0] = '\0';
    pBuf->returnedHeaderSize = 1;
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
    if (pBuf == NULL)
    {
        // Nothing to do
        return;
    }
    
    // Free data
    if (pBuf->data != NULL && pBuf->allocatedSize > 0)
    {
        free(pBuf->data);
        pBuf->data = NULL;
    }
    
    // Free headers
    if (pBuf->pReturnedHeaders != NULL)
    {
        free(pBuf->pReturnedHeaders);
        pBuf->pReturnedHeaders = NULL;
    }
    
    // Free the actual struct
    free(pBuf);
}


/******************
 * Getter and setter functions
 ******************/

long gdrive_dlbuf_get_httpResp(Gdrive_Download_Buffer* pBuf)
{
    return pBuf->httpResp;
}

const char* gdrive_dlbuf_get_data(Gdrive_Download_Buffer* pBuf)
{
    return pBuf->data;
}


/******************
 * Other accessible functions
 ******************/

CURLcode gdrive_dlbuf_download(Gdrive_Download_Buffer* pBuf, CURL* curlHandle)
{
    // Make sure data gets written at the start of the buffer.
    pBuf->usedSize = 0;
    
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
    
    // Capture the returned headers with a callback
    curl_easy_setopt(curlHandle, 
                     CURLOPT_HEADERFUNCTION, 
                     gdrive_dlbuf_header_callback
            );
    curl_easy_setopt(curlHandle, CURLOPT_HEADERDATA, pBuf);
    
    // Do the transfer.
    pBuf->resultCode = curl_easy_perform(curlHandle);
    
    // Get the HTTP response
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &(pBuf->httpResp));
    
    return pBuf->resultCode;
}

int gdrive_dlbuf_download_with_retry(Gdrive_Download_Buffer* pBuf, 
                                     CURL* curlHandle,
                                     bool retryOnAuthError,
                                     int tryNum,
                                     int maxTries
)
{
    CURLcode curlResult = gdrive_dlbuf_download(pBuf, curlHandle);

    
    if (curlResult != CURLE_OK)
    {
        // Download error
        return -1;
    }
    if (gdrive_dlbuf_get_httpResp(pBuf) >= 400)
    {
        // Handle HTTP error responses.  Normal error handling - 5xx gets 
        // retried, 403 gets retried if it's due to rate limits, 401 gets
        // retried after refreshing auth.  If retryOnAuthError is false, 
        // suppress the normal behavior for 401 and don't retry.
        
        // See whether we've already used our maximum attempts.
        if (tryNum == maxTries)
        {
            return -1;
        }
        
        bool retry = false;
        switch (gdrive_dlbuf_retry_on_error(pBuf, gdrive_dlbuf_get_httpResp(pBuf)))
        {
        case GDRIVE_RETRY_RETRY:
            // Normal retry, use exponential backoff.
            gdrive_exponential_wait(tryNum);
            retry = true;
            break;

        case GDRIVE_RETRY_RENEWAUTH:
            // Authentication error, probably expired access token.
            // If retryOnAuthError is true, refresh auth and retry (unless auth 
            // fails).
            if (retryOnAuthError)
            {
                retry = (gdrive_auth() == 0);
                break;
            }
            // else fall through
            
        case GDRIVE_RETRY_NORETRY:
        default:
            retry = false;
            break;
        }
        
        if (retry)
        {
            return gdrive_dlbuf_download_with_retry(pBuf, 
                                                    curlHandle,
                                                    retryOnAuthError,
                                                    tryNum + 1,
                                                    maxTries
                    );
        }
        else
        {
            return -1;
        }
    }
    
    // If we're here, we have a good response.  Return success.
    return 0;
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

static size_t
gdrive_dlbuf_header_callback(char* buffer, size_t size, size_t nitems, 
                             void* userdata)
{
    Gdrive_Download_Buffer* pDlBuf = (Gdrive_Download_Buffer*) userdata;
    
    // Header data in passed in may not be null terminated or end in a newline, 
    // so allow space for the newline and null terminator.
    size_t oldSize = pDlBuf->returnedHeaderSize;
    oldSize = (oldSize > 0) ? oldSize : 1;
    size_t newHeaderLength = size * nitems;
    size_t totalSize = oldSize + newHeaderLength + 1;
    pDlBuf->pReturnedHeaders = realloc(pDlBuf->pReturnedHeaders, totalSize);
    if (pDlBuf->pReturnedHeaders == NULL)
    {
        // Memory error
        return 0;
    }
    pDlBuf->returnedHeaderSize = totalSize;
    
    strncpy(pDlBuf->pReturnedHeaders + oldSize - 1, buffer, newHeaderLength);
    pDlBuf->pReturnedHeaders[totalSize - 2] = '\n';
    pDlBuf->pReturnedHeaders[totalSize -1] = '\0';
    
    return newHeaderLength;
}

static enum Gdrive_Retry_Method 
gdrive_dlbuf_retry_on_error(Gdrive_Download_Buffer* pBuf, long httpResp)
{
    // TODO:    Currently only handles 403 errors correctly when pBuf->fh is 
    //          NULL (when the downloaded data is stored in-memory, not in a 
    //          file).
          
    
    // Most transfers should retry:
    // A. After HTTP 5xx errors, using exponential backoff
    // B. After HTTP 403 errors with a reason of "rateLimitExceeded" or 
    //    "userRateLimitExceeded", using exponential backoff
    // C. After HTTP 401, after refreshing credentials
    // If not one of the above cases, should not retry.
    
    if (httpResp >= 500)
    {
        // Always retry these
        return GDRIVE_RETRY_RETRY;
    }
    else if (httpResp == 401)
    {
        // Always refresh credentials for 401.
        return GDRIVE_RETRY_RENEWAUTH;
    }
    else if (httpResp == 403)
    {
        // Retry ONLY if the reason for the 403 was an exceeded rate limit
        bool retry = false;
        int reasonLength = strlen(GDRIVE_403_USERRATELIMIT) + 1;
        char* reason = malloc(reasonLength);
        if (reason == NULL)
        {
            // Memory error
            return -1;
        }
        reason[0] = '\0';
        Gdrive_Json_Object* pRoot = gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
        if (pRoot != NULL)
        {
            Gdrive_Json_Object* pErrors = 
                    gdrive_json_get_nested_object(pRoot, "error/errors");
            gdrive_json_get_string(pErrors, "reason", reason, reasonLength);
            if ((strcmp(reason, GDRIVE_403_RATELIMIT) == 0) || 
                    (strcmp(reason, GDRIVE_403_USERRATELIMIT) == 0))
            {
                // Rate limit exceeded, retry.
                retry = true;
            }
            // else do nothing (retry remains false for all other 403 
            // errors)

            // Cleanup
            gdrive_json_kill(pRoot);
        }
        free(reason);
        if (retry)
        {
            return GDRIVE_RETRY_RENEWAUTH;
        }
    }
    
    // For all other errors, don't retry.
    return GDRIVE_RETRY_NORETRY;
}

static void gdrive_exponential_wait(int tryNum)
{
    // Number of milliseconds to wait before retrying
    long waitTime;
    int i;
    // Start with 2^tryNum seconds.
    for (i = 0, waitTime = 1000; i < tryNum; i++, waitTime *= 2)
        ;   // No loop body.
    // Randomly add up to 1 second more.
    waitTime += (rand() % 1000) + 1;
    // Convert waitTime to a timespec for use with nanosleep.
    struct timespec waitTimeNano;
    waitTimeNano.tv_sec = waitTime / 1000;  // Integer division
    waitTimeNano.tv_nsec = (waitTime % 1000) * 1000000L;
    nanosleep(&waitTimeNano, NULL);
}



// Just for temporary debugging purposes. This might be kept around and moved to
// a more appropriate place, or it might be removed.
void gdrive_dlbuf_print_headers(const Gdrive_Download_Buffer* pBuf)
{
    puts(pBuf->pReturnedHeaders);
}
