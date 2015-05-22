
#include "gdrive-fileinfo.h"

#include "gdrive-info.h"
#include "gdrive-cache.h"
#include "gdrive-file.h"

#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <math.h>



/*************************************************************************
 * Constants needed only internally within this file
 *************************************************************************/

#define GDRIVE_MIMETYPE_FOLDER "application/vnd.google-apps.folder"


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

static int gdrive_rfc3339_to_epoch_timens(const char* rfcTime, 
                                          struct timespec* pResultTime
);



/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors and destructors
 ******************/

const Gdrive_Fileinfo* gdrive_finfo_get_by_id(const char* fileId)
{
    // Get the information from the cache, or put it in the cache if it isn't
    // already there.
    bool alreadyCached = false;
    
    Gdrive_Fileinfo* pFileinfo = 
            gdrive_cache_get_item(fileId, true, &alreadyCached);
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
    // else it wasn't cached, need to fill in the struct
    
    // Prepare the request
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        // Memory error
        return NULL;
    }
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_GET);
    
    // Add the URL.
    // String to hold the url.  Add 2 to the end to account for the '/' before
    // the file ID, as well as the terminating null.
    char* baseUrl = malloc(strlen(GDRIVE_URL_FILES) + strlen(fileId) + 2);
    if (baseUrl == NULL)
    {
        // Memory error.
        gdrive_xfer_free(pTransfer);
        return NULL;
    }
    strcpy(baseUrl, GDRIVE_URL_FILES);
    strcat(baseUrl, "/");
    strcat(baseUrl, fileId);
    if (gdrive_xfer_set_url(pTransfer, baseUrl) != 0)
    {
        // Error
        free(baseUrl);
        gdrive_xfer_free(pTransfer);
        return NULL;
    }
    free(baseUrl);
    
    // Add query parameters
    if (gdrive_xfer_add_query(pTransfer, "fields", 
                              "title,id,mimeType,fileSize,createdDate,"
                              "modifiedDate,lastViewedByMeDate,parents(id),"
                              "userPermission") != 0)
    {
        // Error
        gdrive_xfer_free(pTransfer);
        return NULL;
    }
    
    // Perform the request
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
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
    Gdrive_Json_Object* pObj = gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
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
        Gdrive_Fileinfo_Array* pFileArray = gdrive_folder_list(fileId);
        if (pFileArray != NULL)
        {
            
            pFileinfo->nChildren = gdrive_finfoarray_get_count(pFileArray);
        }
        gdrive_finfoarray_free(pFileArray);
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
                            Gdrive_Json_Object* pObj
)
{
    long length = 0;
    gdrive_json_realloc_string(pObj, "title", &(pFileinfo->filename), &length);
    length = 0;
    gdrive_json_realloc_string(pObj, "id", &(pFileinfo->id), &length);
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

int gdrive_finfo_real_perms(const Gdrive_Fileinfo* pFileinfo)
{
    // Get the overall system permissions, which are different for a folder
    // or for a regular file.
    int systemPerm = gdrive_get_filesystem_perms(pFileinfo->type);
    
    // Combine the system permissions with the actual file permissions.
    return systemPerm & pFileinfo->basePermission;
}



/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

static int gdrive_rfc3339_to_epoch_timens(const char* rfcTime, 
                                          struct timespec* pResultTime
)
{
    // Get the time down to seconds. Don't do anything with it yet, because
    // we still need to confirm the timezone.
    struct tm epochTime = {0};
    char* remainder = strptime(rfcTime, "%Y-%m-%dT%H:%M:%S", &epochTime);
    if (remainder == NULL)
    {
        // Conversion failure.  
        return -1;
    }
    
    // Get the fraction of a second.  The remainder variable points to the next 
    // character after seconds.  If and only if there are fractional seconds 
    // (which Google Drive does use but which are optional per the RFC 3339 
    // specification),  this will be the '.' character.
    if (*remainder == '.')
    {
        // Rather than getting the integer after the decimal and needing to 
        // count digits or count leading "0" characters, it's easier just to
        // get a floating point (or double) fraction between 0 and 1, then
        // multiply by 1000000000 to get nanoseconds.
        char* start = remainder;
        pResultTime->tv_nsec = lround(1000000000L * strtod(start, &remainder));
    }
    else
    {
        // No fractional part.
        pResultTime->tv_nsec = 0;
    }
    
    // Get the timezone offset from UTC. Google Drive appears to use UTC (offset
    // is "Z"), but I don't know whether that's guaranteed. If not using UTC,
    // the offset will start with either '+' or '-'.
    if (*remainder != '+' && *remainder != '-' && toupper(*remainder) != 'Z')
    {
        // Invalid offset.
        return -1;
    }
    if (toupper(*remainder) != 'Z')
    {
        // Get the hour portion of the offset.
        char* start = remainder;
        long offHour = strtol(start, &remainder, 10);
        if (remainder != start + 2 || *remainder != ':')
        {
            // Invalid offset, not in the form of "+HH:MM" / "-HH:MM"
            return -1;
        }
        
        // Get the minute portion of the offset
        start = remainder + 1;
        long offMinute = strtol(start, &remainder, 10);
        if (remainder != start + 2)
        {
            // Invalid offset, minute isn't a 2-digit number.
            return -1;
        }
        
        // Subtract the offset from the hour/minute parts of the tm struct.
        // This may give out-of-range values (e.g., tm_hour could be -2 or 26),
        // but mktime() is supposed to handle those.
        epochTime.tm_hour -= offHour;
        epochTime.tm_min -= offMinute;
    }
    
    // Convert the broken-down time into seconds.
    pResultTime->tv_sec = mktime(&epochTime);
    
    // Failure if mktime returned -1, success otherwise.
    return pResultTime->tv_sec != (time_t)-1;
    
    
}



