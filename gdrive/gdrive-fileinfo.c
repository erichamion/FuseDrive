

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <curl/curl.h>
#include <string.h>

#include "gdrive.h"
#include "gdrive-internal.h"
#include "gdrive-fileinfo.h"


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/




/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors and destructors
 ******************/

const Gdrive_Fileinfo* gdrive_finfo_get_by_id(Gdrive_Info* pInfo, 
                             const char* fileId
)
{
    // Get the information from the cache, or put it in the cache if it isn't
    // already there.
    bool alreadyCached = false;
    
    Gdrive_Fileinfo* pFileinfo = 
            gdrive_cache_get_item(pInfo->pInternalInfo->pCache, fileId, 
                                  true, &alreadyCached
            );
    if (pFileinfo == NULL)
    {
        // An error occurred, probably out of memory.
        return NULL;
    }
    
    if (alreadyCached)
    {
        // Don't need to do anything else.
        return pFileinfo;
    }
    
    // Convenience assignment
    CURL* curlHandle = pInfo->pInternalInfo->curlHandle;
    
    Gdrive_Query* pQuery = gdrive_query_create(curlHandle);
    int success = gdrive_query_add(pQuery, "fields", 
            "title,id,mimeType,fileSize,createdDate,modifiedDate,"
            "lastViewedByMeDate,parents(id),userPermission"
    );
    if (success != 0)
    {
        // Error, probably memory
        gdrive_query_free(pQuery);
        return NULL;
    }
    
    // String to hold the url.  Add 2 to the end to account for the '/' before
    // the file ID, as well as the terminating null.
    char* baseUrl = malloc(strlen(GDRIVE_URL_FILES) + strlen(fileId) + 2);
    if (baseUrl == NULL)
    {
        // Memory error.
        gdrive_query_free(pQuery);
        return NULL;
    }
    strcpy(baseUrl, GDRIVE_URL_FILES);
    strcat(baseUrl, "/");
    strcat(baseUrl, fileId);
    
    Gdrive_Download_Buffer* pBuf = _gdrive_do_transfer(pInfo, 
                                                       GDRIVE_REQUEST_GET, 
                                                       true, baseUrl, pQuery, 
                                                       NULL, NULL
            );
    gdrive_query_free(pQuery);
    free(baseUrl);
    if (pBuf == NULL)
    {
        // Download error
        return NULL;
    }
    
    if (gdrive_dlbuf_get_httpResp(pBuf) >= 400)
    {
        // Server returned an error that couldn't be retried, or continued
        // returning an error after retrying
        gdrive_dlbuf_free(pBuf);
        return NULL;
    }
    
    // If we're here, we have a good response.  Extract the ID from the 
    // response.
    
    // Convert to a JSON object.
    gdrive_json_object* pObj = gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
    gdrive_dlbuf_free(pBuf);
    if (pObj == NULL)
    {
        // Couldn't convert to JSON object.
        gdrive_dlbuf_free(pBuf);
        return NULL;
    }
    
    gdrive_finfo_read_json(pFileinfo, pObj);
    gdrive_json_kill(pObj);
    
    // If it's a folder, get the number of children.
    if (pFileinfo->type == GDRIVE_FILETYPE_FOLDER)
    {
        Gdrive_Fileinfo_Array* pFileArray = gdrive_fileinfo_array_create();
        if (pFileArray == NULL)
        {
            // Memory error
            return NULL;
        }
        if (gdrive_folder_list(pInfo, fileId, pFileArray) != -1)
        {
            
            pFileinfo->nChildren = pFileArray->nItems;
        }
        gdrive_fileinfo_array_free(pFileArray);
    }
    return pFileinfo;
}

void gdrive_finfo_cleanup(Gdrive_Fileinfo* pFileinfo)
{
    free(pFileinfo->id);
    pFileinfo->id = NULL;
    free(pFileinfo->filename);
    pFileinfo->filename = NULL;
    pFileinfo->type = 0;
    pFileinfo->size = 0;
    memset(&(pFileinfo->creationTime), 0, sizeof(struct timespec));
    memset(&(pFileinfo->modificationTime), 0, sizeof(struct timespec));
    memset(&(pFileinfo->accessTime), 0, sizeof(struct timespec));
    pFileinfo->nParents = 0;
    pFileinfo->nChildren = 0;
    
}




/******************
 * Getter and setter functions
 ******************/

// No getter or setter functions. Members can be read directly, and there's no
// need to directly set individual members.


/******************
 * Other accessible functions
 ******************/

void gdrive_finfo_read_json(Gdrive_Fileinfo* pFileinfo, 
                            gdrive_json_object* pObj
)
{
    pFileinfo->filename = gdrive_json_get_new_string(pObj, "title", NULL);
    pFileinfo->id = gdrive_json_get_new_string(pObj, "id", NULL);
    bool success;
    pFileinfo->size = gdrive_json_get_int64(pObj, "fileSize", true, &success);
    if (!success)
    {
        pFileinfo->size = 0;
    }
    
    char* mimeType = gdrive_json_get_new_string(pObj, "mimeType", NULL);
    if (mimeType != NULL)
    {
        if (strcmp(mimeType, GDRIVE_MIMETYPE_FOLDER) == 0)
        {
            // Folder
            pFileinfo->type = GDRIVE_FILETYPE_FOLDER;
        }
        else if (false)
        {
            // TODO: Add any other special file types.  This
            // will likely include Google Docs.
        }
        else
        {
            // Regular file
            pFileinfo->type = GDRIVE_FILETYPE_FILE;
        }
        free(mimeType);
    }
    
    // Get the user's permissions for the file on the Google Drive account.
    char* role = gdrive_json_get_new_string(pObj, "userPermission/role", NULL);
    if (role != NULL)
    {
        int basePerm = 0;
        if (strcmp(role, "owner") == 0)
        {
            // Full read-write access
            basePerm = S_IWOTH | S_IROTH;
        }
        else if (strcmp(role, "writer") == 0)
        {
            // Full read-write access
            basePerm = S_IWOTH | S_IROTH;
        }
        else if (strcmp(role, "reader") == 0)
        {
            // Read-only access
            basePerm = S_IROTH;
        }
        
//        pFileinfo->basePermission = mountPerm & basePerm;
        pFileinfo->basePermission = basePerm;
        
        // Directories need read and execute permissions to be navigable, and 
        // write permissions to create files. 
        if (pFileinfo->type == GDRIVE_FILETYPE_FOLDER)
        {
            pFileinfo->basePermission = S_IROTH | S_IWOTH | S_IXOTH;
        }
        free(role);
    }
    
    char* cTime = gdrive_json_get_new_string(pObj, "createdDate", NULL);
    if (cTime == NULL || 
            gdrive_rfc3339_to_epoch_timens
            (cTime, &(pFileinfo->creationTime)) == 0)
    {
        // Didn't get a createdDate or failed to convert it.
        memset(&(pFileinfo->creationTime), 0, sizeof(struct timespec));
    }
    free(cTime);
    
    char* mTime = gdrive_json_get_new_string(pObj, "modifiedDate", NULL);
    if (mTime == NULL || 
            gdrive_rfc3339_to_epoch_timens
            (mTime, &(pFileinfo->modificationTime)) == 0)
    {
        // Didn't get a modifiedDate or failed to convert it.
        memset(&(pFileinfo->modificationTime), 0, sizeof(struct timespec));
    }
    free(mTime);
    
    char* aTime = gdrive_json_get_new_string(pObj, 
            "lastViewedByMeDate", 
            NULL
    );
    if (aTime == NULL || 
            gdrive_rfc3339_to_epoch_timens
            (aTime, &(pFileinfo->accessTime)) == 0)
    {
        // Didn't get an accessed date or failed to convert it.
        memset(&(pFileinfo->accessTime), 0, sizeof(struct timespec));
    }
    free(aTime);
    
    pFileinfo->nParents = gdrive_json_array_length(pObj, "parents");
}




/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/



