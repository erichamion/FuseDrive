
#include "gdrive-cache.h"

#include <string.h>
#include <assert.h>







/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_Cache
{
    //Gdrive_Info* pInfo;
    time_t cacheTTL;
    time_t lastUpdateTime;
    int64_t nextChangeId;
    Gdrive_Cache_Node* pCacheHead;
    Gdrive_Fileid_Cache_Node* pFileIdCacheHead; 
} Gdrive_Cache;

static Gdrive_Cache* gdrive_cache_get_internal(void);

static void gdrive_cache_remove_id(const char* fileId);





/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors, factory methods, destructors and similar
 ******************/
int gdrive_cache_init(time_t cacheTTL)
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    
    // The first time this function is called, lastUpdateTime will be 0. It will
    // never be 0 again (unless the user travels back in time to 1970, bringing
    // the internet and Google back with him/her), so that's a good test for 
    // whether or not the cache has been initialized.
    if (pCache->lastUpdateTime > 0)
    {
        // Already initialized, nothing to do
        return 0;
    }
    // else not initialized yet
    
    pCache->cacheTTL = cacheTTL;
    
    // Prepare and send the network request
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
            gdrive_xfer_add_query(pTransfer, "fields", "largestChangeId")
        )
    {
        // Error
        gdrive_xfer_free(pTransfer);
        return -1;
    }
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    bool success = false;
    if (pBuf != NULL && gdrive_dlbuf_get_httpresp(pBuf) < 400)
    {
        // Response was good, try extracting the data.
        Gdrive_Json_Object* pObj = gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
        if (pObj != NULL)
        {
            pCache->nextChangeId = 
                    gdrive_json_get_int64(pObj, "largestChangeId", 
                                          true, &success
                    ) + 1;
            gdrive_json_kill(pObj);
        }
    }
    
    gdrive_dlbuf_free(pBuf);
    if (success)
    {
        return 0;
    }
    else
    {
        // Some error occurred.
        //free(pCache);
        return -1;
    }
}

const Gdrive_Cache* gdrive_cache_get(void)
{
    return gdrive_cache_get_internal();
}

void gdrive_cache_cleanup(void)
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    gdrive_fidnode_clear_all(pCache->pFileIdCacheHead);
    pCache->pFileIdCacheHead = NULL;
    gdrive_cnode_free_all(pCache->pCacheHead);
    pCache->pCacheHead = NULL;
}



/******************
 * Getter and setter functions
 ******************/

Gdrive_Fileid_Cache_Node* gdrive_cache_get_fileidcachehead()
{
    return gdrive_cache_get()->pFileIdCacheHead;
}

time_t gdrive_cache_get_ttl()
{
    return gdrive_cache_get()->cacheTTL;
}

time_t gdrive_cache_get_lastupdatetime()
{
    return gdrive_cache_get()->lastUpdateTime;
}

int64_t gdrive_cache_get_nextchangeid()
{
    return gdrive_cache_get()->nextChangeId;
}


/******************
 * Other accessible functions
 ******************/

int gdrive_cache_update_if_stale()
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    if (pCache->lastUpdateTime + pCache->cacheTTL < time(NULL))
    {
        return gdrive_cache_update(pCache);
    }
    
    return 0;
}

int gdrive_cache_update()
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    
    // Convert the numeric largest change ID into a string
    char* changeIdString = NULL;
    size_t changeIdStringLen = snprintf(NULL, 0, "%lu", pCache->nextChangeId);
    changeIdString = malloc(changeIdStringLen + 1);
    if (changeIdString == NULL)
    {
        // Memory error
        return -1;
    }
    snprintf(changeIdString, changeIdStringLen + 1, "%lu", 
            pCache->nextChangeId
            );
    
    // Prepare the request, using the string change ID, and send it
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        // Memory error
        free(changeIdString);
        return -1;
    }
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_GET);
    if (
            gdrive_xfer_set_url(pTransfer, GDRIVE_URL_CHANGES) ||
            gdrive_xfer_add_query(pTransfer, "startChangeId", changeIdString) ||
            gdrive_xfer_add_query(pTransfer, "includeSubscribed", "false") /* ||
            gdrive_xfer_add_query(pTransfer, "fields", 
                                  "largestChangeId,items(fileId,deleted,file)")       */
        )
    {
        // Error
        free(changeIdString);
        gdrive_xfer_free(pTransfer);
    }
    free(changeIdString);
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    
    int returnVal = -1;
    if (pBuf != NULL && gdrive_dlbuf_get_httpresp(pBuf) < 400)
    {
        // Response was good, try extracting the data.
        Gdrive_Json_Object* pObj = gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
        if (pObj != NULL)
        {
            // Update or remove cached data for each item in the "items" array.
            Gdrive_Json_Object* pChangeArray = 
                    gdrive_json_get_nested_object(pObj, "items");
            int arraySize = gdrive_json_array_length(pChangeArray, NULL);
            for (int i = 0; i < arraySize; i++)
            {
                Gdrive_Json_Object* pItem = 
                        gdrive_json_array_get(pChangeArray, NULL, i);
                if (pItem == NULL)
                {
                    // Couldn't get this item, skip to the next one.
                    continue;
                }
                char* fileId = 
                        gdrive_json_get_new_string(pItem, "fileId", NULL);
                if (fileId == NULL)
                {
                    // Couldn't get an ID for the changed file, skip to the
                    // next one.
                    continue;
                }
                
                // We don't know whether the file has been renamed or moved,
                // so remove it from the fileId cache.
                gdrive_fidnode_remove_by_id(&pCache->pFileIdCacheHead, fileId);
                
                // Update the file metadata cache, but only if the file is not
                // opened for writing with dirty data.
                Gdrive_Cache_Node* pCacheNode = 
                        gdrive_cnode_get(NULL,
                                               &(pCache->pCacheHead), 
                                               fileId, 
                                               false, 
                                               NULL
                        );
                if (pCacheNode != NULL && !gdrive_cnode_is_dirty(pCacheNode))
                {
                    // If this file was in the cache, update its information
                    gdrive_cnode_update_from_json(
                            pCacheNode, 
                            gdrive_json_get_nested_object(pItem, "file")
                            );
                }
                // else either not in the cache, or there is dirty data we don't
                // want to overwrite.
                
                
                // The file's parents may now have a different number of 
                // children.  Remove the parents from the cache.
                int numParents = gdrive_json_array_length(pItem, "file/parents");
                for (int nParent = 0; nParent < numParents; nParent++)
                {
                    // Get the fileId of the current parent in the array.
                    char* parentId = NULL;
                    Gdrive_Json_Object* pParentObj = 
                            gdrive_json_array_get(pItem, "file/parents", nParent);
                    if (pParentObj != NULL)
                    {
                        parentId = gdrive_json_get_new_string(pParentObj, 
                                                                "id", 
                                                                NULL);
                    }
                    // Remove the parent from the cache, if present.
                    if (parentId != NULL)
                    {
                        gdrive_cache_remove_id(parentId);
                    }
                    free(parentId);
                }
                
                free(fileId);
            }
            
            bool success = false;
            int64_t nextChangeId = gdrive_json_get_int64(pObj, 
                                                         "largestChangeId", 
                                                         true, &success
                    ) + 1;
            if (success)
            {
                pCache->nextChangeId = nextChangeId;
            }
            returnVal = success ? 0 : -1;
            gdrive_json_kill(pObj);
        }
    }
    
    // Reset the last updated time
    pCache->lastUpdateTime = time(NULL);
    
    gdrive_dlbuf_free(pBuf);
    return returnVal;
}

Gdrive_Fileinfo* gdrive_cache_get_item(const char* fileId,
                                       bool addIfDoesntExist,
                                       bool* pAlreadyExists
)
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    
    // Get the existing node (or a new one) from the cache.
    Gdrive_Cache_Node* pNode = gdrive_cnode_get(NULL,
                                                &(pCache->pCacheHead),
                                                fileId, 
                                                addIfDoesntExist, 
                                                pAlreadyExists
            );
    if (pNode == NULL)
    {
        // There was an error, or the node doesn't exist and we aren't allowed
        // to create a new one.
        return NULL;
    }
    
    // Test whether the cached information is too old.  Use last updated time
    // for either the individual node or the entire cache, whichever is newer.
    // If the node's update time is 0, always update it.
    time_t cacheUpdated = pCache->lastUpdateTime;
    time_t nodeUpdated = gdrive_cnode_get_update_time(pNode);
    time_t expireTime = (nodeUpdated > cacheUpdated ? 
        nodeUpdated : cacheUpdated) + pCache->cacheTTL;
    if (expireTime < time(NULL) || nodeUpdated == (time_t) 0)
    {
        // Update the cache and try again.
        
        // Folder nodes may be deleted by cache updates, but regular file nodes
        // are safe.
        bool isFolder = (gdrive_cnode_get_filetype(pNode) == 
                GDRIVE_FILETYPE_FOLDER);
        
        gdrive_cache_update();
        
        return (isFolder ? 
                gdrive_cache_get_item(fileId, addIfDoesntExist, 
                                      pAlreadyExists) :
                gdrive_cnode_get_fileinfo(pNode));
    }
    
    // We have a good node that's not too old.
    return gdrive_cnode_get_fileinfo(pNode);
}

int gdrive_cache_add_fileid(const char* path, const char* fileId)
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    return gdrive_fidnode_add(&(pCache->pFileIdCacheHead), path, fileId);
}

Gdrive_Cache_Node* gdrive_cache_get_node(const char* fileId, 
                                         bool addIfDoesntExist, 
                                         bool* pAlreadyExists
)
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    return gdrive_cnode_get(NULL, &(pCache->pCacheHead), fileId,
                                  addIfDoesntExist, pAlreadyExists
            );
}

char* gdrive_cache_get_fileid(const char* path)
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    
    // Get the cached node if it exists.  If it doesn't exist, fail.
    Gdrive_Fileid_Cache_Node* pHeadNode = pCache->pFileIdCacheHead;
    Gdrive_Fileid_Cache_Node* pNode = gdrive_fidnode_get_node(pHeadNode, path);
    if (pNode == NULL)
    {
        // The path isn't cached.  Return null.
        return NULL;
    }
    
    // We have the cached item.  Test whether it's too old.  Use the last update
    // either of the entire cache, or of the individual item, whichever is
    // newer.
    time_t cacheUpdateTime = gdrive_cache_get_lastupdatetime(pCache);
    time_t nodeUpdateTime = gdrive_fidnode_get_lastupdatetime(pNode);
    time_t cacheTTL = gdrive_cache_get_ttl(pCache);
    time_t expireTime = ((nodeUpdateTime > cacheUpdateTime) ?
        nodeUpdateTime : cacheUpdateTime) + cacheTTL;
    if (time(NULL) > expireTime)
    {
        // Item is expired.  Check for updates and try again.
        gdrive_cache_update(pCache);
        return gdrive_cache_get_fileid(path);
    }
    
    return gdrive_fidnode_get_fileid(pNode);
}

void gdrive_cache_delete_id(const char* fileId)
{
    assert(fileId != NULL);
    
    Gdrive_Cache* pCache = gdrive_cache_get_internal();

    // Remove the ID from the file Id cache
    gdrive_fidnode_remove_by_id(&pCache->pFileIdCacheHead, fileId);
    
    // If the file isn't opened by anybody, delete it from the cache 
    // immediately. Otherwise, mark it for delete on close.
            
    // Find the node we want to remove.
    Gdrive_Cache_Node* pNode = 
            gdrive_cnode_get(NULL, &(pCache->pCacheHead), fileId, 
                                   false, NULL);
    if (pNode == NULL)
    {
        // Didn't find it.  Do nothing.
        return;
    }
    gdrive_cnode_mark_deleted(pNode, &pCache->pCacheHead);
}

void gdrive_cache_delete_node(Gdrive_Cache_Node* pNode)
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    gdrive_cnode_delete(pNode, &pCache->pCacheHead);
}


/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

static Gdrive_Cache* gdrive_cache_get_internal(void)
{
    static Gdrive_Cache cache;
    return &cache;
}

static void gdrive_cache_remove_id(const char* fileId)
{
    Gdrive_Cache* pCache = gdrive_cache_get_internal();
    // Find the node we want to remove.
    Gdrive_Cache_Node* pNode = 
            gdrive_cnode_get(NULL, &(pCache->pCacheHead), fileId, 
                                   false, NULL);
    if (pNode == NULL)
    {
        // Didn't find it.  Do nothing.
        return;
    }
    

    gdrive_cnode_delete(pNode, &(pCache->pCacheHead));
}



