
#include "gdrive-sysinfo.h"

#include "gdrive-info.h"
#include "gdrive-cache.h"

#include <string.h>
#include <stdbool.h>
    

typedef struct Gdrive_Sysinfo
{
    // nextChangeId: For internal use
    int64_t nextChangeId;
    // quotaBytesTotal: Total space on Google Drive, in bytes
    int64_t quotaBytesTotal;
    // quotaBytesUsed: Space already used, in bytes
    int64_t quotaBytesUsed;
    // rootId: Google Drive file ID for the root folder
    char* rootId;
    
} Gdrive_Sysinfo;    


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

static const Gdrive_Sysinfo* gdrive_sysinfo_get_or_clear(bool cleanup);

static void gdrive_sysinfo_cleanup_internal(Gdrive_Sysinfo* pSysinfo);

static int gdrive_sysinfo_fill_from_json(Gdrive_Sysinfo* pDest, 
                                         Gdrive_Json_Object* pObj);

static int gdrive_sysinfo_update(Gdrive_Sysinfo* pDest);


/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors, factory methods, destructors and similar
 ******************/

// No constructors. This is a single struct instance that lives
// in static memory for the lifetime of the application. Members are retrieved
// using the gdrive_sysinfo_get_*() functions below.

void gdrive_sysinfo_cleanup()
{
    gdrive_sysinfo_get_or_clear(true);
}


/******************
 * Getter and setter functions
 ******************/

int64_t gdrive_sysinfo_get_size(void)
{
    return gdrive_sysinfo_get_or_clear(false)->quotaBytesTotal;
}

int64_t gdrive_sysinfo_get_used()
{
    return gdrive_sysinfo_get_or_clear(false)->quotaBytesUsed;
}

const char* gdrive_sysinfo_get_rootid(void)
{
    return gdrive_sysinfo_get_or_clear(false)->rootId;
}


/******************
 * Other accessible functions
 ******************/

// No other public functions


/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

static const Gdrive_Sysinfo* gdrive_sysinfo_get_or_clear(bool cleanup)
{
    // Set the initial nextChangeId to the lowest possible value, guaranteeing
    // that the info will be updated the first time this function is called.
    static Gdrive_Sysinfo sysinfo = {.nextChangeId = INT64_MIN};
    
    if (cleanup)
    {
        // Clear out the struct and return NULL
        gdrive_sysinfo_cleanup_internal(&sysinfo);
        return NULL;
    }
    
    

    // Is the info current?
    // First, make sure the cache is up to date.
    gdrive_cache_update_if_stale(gdrive_cache_get());

    // If the Sysinfo's next change ID is at least as high as the cache's
    // next change ID, then our info is current.  No need to do anything
    // else. Otherwise, it needs updated.
    int64_t cacheChangeId = 
            gdrive_cache_get_nextchangeid(gdrive_cache_get());
    if (sysinfo.nextChangeId < cacheChangeId)
    {
        // Either we don't have any sysinfo, or it needs updated.
        gdrive_sysinfo_update(&sysinfo);
    }
    
    
    return &sysinfo;
}

static void gdrive_sysinfo_cleanup_internal(Gdrive_Sysinfo* pSysinfo)
{
    free(pSysinfo->rootId);
    memset(pSysinfo, 0, sizeof(Gdrive_Sysinfo));
    pSysinfo->nextChangeId = INT64_MIN;
}

static int gdrive_sysinfo_fill_from_json(Gdrive_Sysinfo* pDest, 
                                         Gdrive_Json_Object* pObj)
{
    bool currentSuccess = true;
    bool totalSuccess = true;
    pDest->nextChangeId = gdrive_json_get_int64(pObj, 
                                                "largestChangeId", 
                                                true, 
                                                &currentSuccess
            ) + 1;
    totalSuccess = totalSuccess && currentSuccess;
    
    pDest->quotaBytesTotal = gdrive_json_get_int64(pObj, 
                                                   "quotaBytesTotal", 
                                                   true,
                                                   &currentSuccess
            );
    totalSuccess = totalSuccess && currentSuccess;
    
    pDest->quotaBytesUsed = gdrive_json_get_int64(pObj, 
                                                  "quotaBytesUsed", 
                                                   true,
                                                   &currentSuccess
            );
    totalSuccess = totalSuccess && currentSuccess;
    
    pDest->rootId = gdrive_json_get_new_string(pObj, "rootFolderId", NULL);
    currentSuccess = totalSuccess && (pDest->rootId != NULL);
    
    // For now, we'll ignore the importFormats and exportFormats.
    
    return totalSuccess ? 0 : -1;
}

static int gdrive_sysinfo_update(Gdrive_Sysinfo* pDest)
{
    if (pDest != NULL)
    {
        // Clean up the existing info.
        gdrive_sysinfo_cleanup_internal(pDest);
    }
        
    const char* const fieldString = "quotaBytesTotal,quotaBytesUsed,"
            "largestChangeId,rootFolderId,importFormats,exportFormats";
    
    // Prepare the transfer
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        // Memory error
        return -1;
    }
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_GET);
    if (
            gdrive_xfer_set_url(pTransfer, GDRIVE_URL_ABOUT) || 
            gdrive_xfer_add_query(pTransfer, "includeSubscribed", "false") || 
            gdrive_xfer_add_query(pTransfer, "fields", fieldString)
        )
    {
        // Error
        gdrive_xfer_free(pTransfer);
        return -1;
    }
    
    // Do the transfer.
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    int returnVal = -1;
    if (pBuf != NULL && gdrive_dlbuf_get_httpresp(pBuf) < 400)
    {
        // Response was good, try extracting the data.
        Gdrive_Json_Object* pObj = 
                gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
        if (pObj != NULL)
        {
            returnVal = gdrive_sysinfo_fill_from_json(pDest, pObj);
        }
        gdrive_json_kill(pObj);
    }
    
    gdrive_dlbuf_free(pBuf);
    
    return returnVal;
}


