


#include "gdrive-transfer-descriptor.h"
#include "gdrive-query.h"
#include "gdrive-download-buffer.h"

#include <string.h>


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
    struct curl_slist* pHeaders;
    FILE* destFile;
    gdrive_xfer_upload_callback uploadCallback;
} Gdrive_Transfer;

typedef struct Gdrive_Transfer_Userdata
{
    void* userdata;
    off_t position;
} Gdrive_Transfer_Userdata;



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

enum Gdrive_Request_Type gdrive_xfer_get_requesttype(Gdrive_Transfer* pTransfer)
{
    return pTransfer->requestType;
}

void gdrive_xfer_set_retryonautherror(Gdrive_Transfer* pTransfer, bool retry)
{
    pTransfer->retryOnAuthError = retry;
}

bool gdrive_xfer_get_retryonautherror(Gdrive_Transfer* pTransfer)
{
    return pTransfer->retryOnAuthError;
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

const char* gdrive_xfer_get_url(Gdrive_Transfer* pTransfer)
{
    return pTransfer->url;
}

void gdrive_xfer_set_destfile(Gdrive_Transfer* pTransfer, FILE* destFile)
{
    pTransfer->destFile = destFile;
}



/******************
 * Other accessible functions
 ******************/



/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/




