
#include "gdrive-sysinfo.h"

#include "gdrive-info.h"
#include "gdrive-cache.h"

#include <string.h>
    

typedef struct Gdrive_Sysinfo
{
    int64_t nextChangeId;       // For internal use
    int64_t quotaBytesTotal;    // Total space on Google Drive, in bytes
    int64_t quotaBytesUsed;     // Space already used, in bytes
    char* rootId;               // Google Drive file ID for the root folder
    
} Gdrive_Sysinfo;    


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

static const Gdrive_Sysinfo* gdrive_get_sysinfo(void);

static void gdrive_sysinfo_cleanup(Gdrive_Sysinfo* pSysinfo);

static int gdrive_sysinfo_fill_from_json(Gdrive_Sysinfo* pDest, 
                                   Gdrive_Json_Object* pObj
);

static int gdrive_sysinfo_update(Gdrive_Sysinfo* pDest);




/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors and destructors
 ******************/

// No constructors or destructors. This is a single struct instance that lives
// in static memory for the lifetime of the application. Members are retrieved
// using the gdrive_sysinfo_get_*() functions below.




/******************
 * Getter and setter functions
 ******************/

int64_t gdrive_sysinfo_get_size(void)
{
    return gdrive_get_sysinfo()->quotaBytesTotal;
}

int64_t gdrive_sysinfo_get_used()
{
    return gdrive_get_sysinfo()->quotaBytesUsed;
}

const char* gdrive_sysinfo_get_rootid(void)
{
    return gdrive_get_sysinfo()->rootId;
}


/******************
 * Other accessible functions
 ******************/

// No other public functions


/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

static const Gdrive_Sysinfo* gdrive_get_sysinfo(void)
{
    // Set the initial nextChangeId to the lowest possible value, guaranteeing
    // that the info will be updated the first time this function is called.
    static Gdrive_Sysinfo sysinfo = {.nextChangeId = INT64_MIN};
    
    

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

static void gdrive_sysinfo_cleanup(Gdrive_Sysinfo* pSysinfo)
{
    free(pSysinfo->rootId);
    memset(pSysinfo, 0, sizeof(Gdrive_Sysinfo));
    pSysinfo->nextChangeId = INT64_MIN;
}

static int gdrive_sysinfo_fill_from_json(Gdrive_Sysinfo* pDest, 
                                   Gdrive_Json_Object* pObj
)
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
        gdrive_sysinfo_cleanup(pDest);
    }
        
    const char* const fieldString = "quotaBytesTotal,quotaBytesUsed,"
            "largestChangeId,rootFolderId,importFormats,exportFormats";
    
    Gdrive_Query* pQuery = NULL;
    pQuery = gdrive_query_add(pQuery, "includeSubscribed", "false");
    pQuery = gdrive_query_add(pQuery, "fields", fieldString);
    if (pQuery == NULL)
    {
        // Memory error
        return -1;
    }
    
    // Do the transfer.
    Gdrive_Download_Buffer* pBuf = 
            gdrive_do_transfer(GDRIVE_REQUEST_GET, true,
                               GDRIVE_URL_ABOUT, pQuery, NULL, NULL
            );
    gdrive_query_free(pQuery);
    
    int returnVal = -1;
    if (pBuf != NULL && gdrive_dlbuf_get_httpResp(pBuf) < 400)
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


