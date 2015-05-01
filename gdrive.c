/* 
 * File:   GDrive.c
 * Author: me
 *
 * Created on December 29, 2014, 8:34 AM
 */

#define _XOPEN_SOURCE 500

#include <curl/curl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <fcntl.h>

#include <errno.h>

#include "gdrive-json.h"
#include "gdrive.h"
#include "gdrive-internal.h"

/*
 * gdrive_init():   Initializes the network connection, sets appropriate 
 *                  settings for the Google Drive session, and ensures the user 
 *                  has granted necessary access permissions for the Google 
 *                  Drive account.
 */
int gdrive_init(Gdrive_Info** ppGdriveInfo, 
                int access, 
                const char* authFilename, 
                time_t cacheTTL,
                enum Gdrive_Interaction interactionMode,
                size_t minFileChunkSize,
                int maxChunksPerFile
)
{
    // Allocate memory first.  Otherwise, if memory allocation fails, we won't
    // be able to tell that curl_global_init() was already called.
    int condition;
    if ((condition = _gdrive_info_create(ppGdriveInfo)) != 0)
    {
        return condition;
    }
    
    Gdrive_Info* pInfo = *ppGdriveInfo;
    
    // If necessary, initialize curl.
    if (!(pInfo->pInternalInfo->isCurlInitialized) && 
            curl_global_init(CURL_GLOBAL_ALL) != 0)
    {
        // Curl initialization failed.  For now, return -1.  Later, we
        // may define different error conditions.
        return -1;
    }
    
    // If we've made it this far, both the Gdrive_Info and its contained
    // Gdrive_Info_Internal exist, and curl has been successfully initialized.
    // Signal that fact in the data structure, and defer the rest of the 
    // processing to gdrive_init_nocurl().
    pInfo->pInternalInfo->isCurlInitialized = true;
    return gdrive_init_nocurl(ppGdriveInfo, 
                              access, 
                              authFilename, 
                              cacheTTL,
                              interactionMode,
                              minFileChunkSize,
                              maxChunksPerFile
            );
}

/*
 * gdrive_init_nocurl():    Similar to gdrive_init(), but does NOT initialize
 *                          the network connection (does not call 
 *                          curl_global_init()).
 */
int gdrive_init_nocurl(Gdrive_Info** ppGdriveInfo, 
                       int access, 
                       const char* authFilename, 
                       time_t cacheTTL,
                       enum Gdrive_Interaction interactionMode,
                       size_t minFileChunkSize,
                       int maxChunksPerFile
)
{
    // Seed the RNG.
    srand(time(NULL));
    
    // Allocate any necessary struct memory.
    int condition;
    if ((condition = _gdrive_info_create(ppGdriveInfo)) != 0)
    {
        return condition;
    }
    Gdrive_Info* pInfo = *ppGdriveInfo;
    // Assume curl_global_init() has already been called somewhere.
    pInfo->pInternalInfo->isCurlInitialized = true;
    
    // Can we prompt the user for authentication during initial setup?
    if (interactionMode == GDRIVE_INTERACTION_STARTUP || 
            interactionMode == GDRIVE_INTERACTION_ALWAYS)
    {
        pInfo->settings.userInteractionAllowed = true;
    }
    
    // If a filename was given, attempt to open the file and read its contents.
    if (authFilename != NULL)
    {
        pInfo->settings.authFilename = realpath(authFilename, NULL);
        if (pInfo->settings.authFilename != NULL)
        {
            strcpy(pInfo->settings.authFilename, authFilename);
            _gdrive_read_auth_file(authFilename, pInfo->pInternalInfo);
        }
    }
        
    // Authenticate or refresh access
    pInfo->settings.mode = access;
    if (gdrive_auth(pInfo) != 0)
    {
        // Could not get the required permissions.  Return error.
        return -1;
    }
    gdrive_save_auth(pInfo);
    // Can we continue prompting for authentication if needed later?
    pInfo->settings.userInteractionAllowed = 
            (interactionMode == GDRIVE_INTERACTION_ALWAYS);
    
    // Initialize the cache
    _gdrive_cache_init(pInfo);
    pInfo->settings.cacheTTL = cacheTTL;
    
    // Set chunk size
    pInfo->settings.minChunkSize = (minFileChunkSize > 0) ?
        _gdrive_divide_round_up(minFileChunkSize, GDRIVE_BASE_CHUNK_SIZE) * 
            GDRIVE_BASE_CHUNK_SIZE :
        GDRIVE_BASE_CHUNK_SIZE;
    pInfo->settings.maxChunks = maxChunksPerFile;
    
    return 0;
}

int gdrive_save_auth(Gdrive_Info* pInfo)
{
    if (pInfo->settings.authFilename == NULL || 
            pInfo->settings.authFilename[0] == '\0')
    {
        // Do nothing if there's no filename
        return -1;
    }
    
    // Create a JSON object, fill it with the necessary details, 
    // convert to a string, and write to the file.
    FILE* outFile = fopen(pInfo->settings.authFilename, "w");
    if (outFile == NULL)
    {
        // Couldn't open file for writing.
        return -1;
    }
    
    gdrive_json_object* pObj = gdrive_json_new();
    gdrive_json_add_string(pObj, GDRIVE_FIELDNAME_ACCESSTOKEN, 
                           pInfo->pInternalInfo->accessToken
            );
    gdrive_json_add_string(pObj, GDRIVE_FIELDNAME_REFRESHTOKEN, 
                           pInfo->pInternalInfo->refreshToken
            );
    int success = fputs(gdrive_json_to_string(pObj, true), outFile);
    gdrive_json_kill(pObj);
    fclose(outFile);
    
    return (success >= 0) ? 0 : -1;
}

int gdrive_auth(Gdrive_Info* pInfo)
{
    if (pInfo->pInternalInfo->curlHandle == NULL)
    {
        // Create a new curl easy handle
        pInfo->pInternalInfo->curlHandle = curl_easy_init();
        if (pInfo->pInternalInfo->curlHandle == NULL)
        {
            // Error creating curl easy handle, return error.
            return -1;
        }
    }
    
    // Try to refresh existing tokens first.
    if (pInfo->pInternalInfo->refreshToken != NULL && 
            pInfo->pInternalInfo->refreshToken[0] != '\0')
    {
        int refreshSuccess = _gdrive_refresh_auth_token(
                pInfo, 
                GDRIVE_GRANTTYPE_REFRESH,
                pInfo->pInternalInfo->refreshToken
        );
        
        if (refreshSuccess == 0)
        {
            // Refresh succeeded, but we don't know what scopes were previously
            // granted.  Check to make sure we have the required scopes.  If so,
            // then we don't need to do anything else and can return success.
            int success = _gdrive_check_scopes(pInfo);
            if (success == 0)
            {
                // Refresh succeeded with correct scopes, return success.
                return 0;
            }
        }
    }
    
    // Either didn't have a refresh token, or it didn't work.  Need to get new
    // authorization, if allowed.
    if (!pInfo->settings.userInteractionAllowed)
    {
        // Need to get new authorization, but not allowed to interact with the
        // user.  Return error.
        return -1;
    }
    
    // If we've gotten this far, then we need to interact with the user, and
    // we're allowed to do so.  Prompt for authorization, and return whatever
    // success or failure the prompt returns.
    return _gdrive_prompt_for_auth(pInfo);
}

int gdrive_file_info_from_id(Gdrive_Info* pInfo, 
                             const char* fileId, 
                             Gdrive_Fileinfo** ppFileinfo
)
{
    // Get the information from the cache, or put it in the cache if it isn't
    // already there.
    bool alreadyCached = false;
    *ppFileinfo = _gdrive_cache_get_item(pInfo, fileId, true, &alreadyCached);
    if (*ppFileinfo == NULL)
    {
        // An error occurred, probably out of memory.
        return -1;
    }
    
    if (alreadyCached)
    {
        // Don't need to do anything else.
        return 0;
    }
    
    // Convenience assignments
    Gdrive_Fileinfo* pFileinfo = *ppFileinfo;
    CURL* curlHandle = pInfo->pInternalInfo->curlHandle;
    
    Gdrive_Query* pQuery = _gdrive_query_create(curlHandle);
    int success = _gdrive_query_add(pQuery, "fields", 
            "title,id,mimeType,fileSize,createdDate,modifiedDate,"
            "lastViewedByMeDate,parents(id),userPermission"
    );
    if (success != 0)
    {
        // Error, probably memory
        _gdrive_query_free(pQuery);
        return -1;
    }
    
    // String to hold the url.  Add 2 to the end to account for the '/' before
    // the file ID, as well as the terminating null.
    char* baseUrl = malloc(strlen(GDRIVE_URL_FILES) + strlen(fileId) + 2);
    if (baseUrl == NULL)
    {
        // Memory error.
        _gdrive_query_free(pQuery);
        return -1;
    }
    strcpy(baseUrl, GDRIVE_URL_FILES);
    strcat(baseUrl, "/");
    strcat(baseUrl, fileId);
    
    Gdrive_Download_Buffer* pBuf = _gdrive_do_transfer(pInfo, 
                                                       GDRIVE_REQUEST_GET, 
                                                       true, baseUrl, pQuery, 
                                                       NULL, NULL
            );
    _gdrive_query_free(pQuery);
    free(baseUrl);
    if (pBuf == NULL)
    {
        // Download error
        return -1;
    }
    
    if (pBuf->httpResp >= 400)
    {
        // Server returned an error that couldn't be retried, or continued
        // returning an error after retrying
        _gdrive_download_buffer_free(pBuf);
        return -1;
    }
    
    // If we're here, we have a good response.  Extract the ID from the 
    // response.
    
    // Convert to a JSON object.
    gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
    _gdrive_download_buffer_free(pBuf);
    if (pObj == NULL)
    {
        // Couldn't convert to JSON object.
        _gdrive_download_buffer_free(pBuf);
        return -1;
    }
    
    _gdrive_get_fileinfo_from_json(pInfo, pObj, pFileinfo);
    gdrive_json_kill(pObj);
    
    // If it's a folder, get the number of children.
    if (pFileinfo->type == GDRIVE_FILETYPE_FOLDER)
    {
        Gdrive_Fileinfo_Array* pFileArray = gdrive_fileinfo_array_create();
        if (pFileArray == NULL)
        {
            // Memory error
            return -ENOMEM;
        }
        if (gdrive_folder_list(pInfo, fileId, pFileArray) != -1)
        {
            
            pFileinfo->nChildren = pFileArray->nItems;
        }
        gdrive_fileinfo_array_free(pFileArray);
    }
    return 0;
}

int gdrive_folder_list(Gdrive_Info* pInfo, 
                       const char* folderId, 
                       Gdrive_Fileinfo_Array* pArray
)
{
    CURL* curlHandle = pInfo->pInternalInfo->curlHandle;
    
    if (pArray == NULL || pArray->nItems > 0 || pArray->pArray != NULL)
    {
        // Invalid argument
        return -1;
    }
    pArray->nItems = -1;
    
    // Allow for an initial quote character in addition to the terminating null
    char* filter = malloc(strlen(folderId) + 
                            strlen("' in parents and trashed=false") + 2);
    if (filter == NULL)
    {
        return -1;
    }
    strcpy(filter, "'");
    strcat(filter, folderId);
    strcat(filter, "' in parents and trashed=false");
    
    Gdrive_Query* pQuery = _gdrive_query_create(curlHandle);
    if (pQuery == NULL)
    {
        // Memory error
        free(filter);
        return -1;
    }
    _gdrive_query_add(pQuery, "q", filter);
    _gdrive_query_add(pQuery, "fields", "items(title,id,mimeType)");
    free(filter);
    
    Gdrive_Download_Buffer* pBuf = 
            _gdrive_do_transfer(pInfo, GDRIVE_REQUEST_GET, true, 
                                GDRIVE_URL_FILES, pQuery, NULL, NULL
            );
    _gdrive_query_free(pQuery);
    
    int returnVal = -1;
    if (pBuf != NULL && pBuf->httpResp < 400)
    {
        // Transfer was successful.  Convert result to a JSON object and extract
        // the file meta-info.
        gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
        if (pObj != NULL)
        {
            returnVal = gdrive_json_array_length(pObj, "items");
            if (returnVal > 0)
            {
                // Create the array of Gdrive_Fileinfo structs and initialize
                // it to all 0s.
                pArray->pArray = malloc(returnVal * sizeof(Gdrive_Fileinfo));
                memset(pArray->pArray, 0, returnVal * sizeof(Gdrive_Fileinfo));
                if (pArray->pArray != NULL)
                {
                    // Extract the file info from each member of the array.
                    Gdrive_Fileinfo* infoArray = pArray->pArray;
                    for (int index = 0; index < returnVal; index++)
                    {
                        gdrive_json_object* pFile = gdrive_json_array_get(
                                pObj, 
                                "items", 
                                index
                                );
                        if (pFile != NULL)
                        {
                            _gdrive_get_fileinfo_from_json(pInfo, 
                                                           pFile, 
                                                           infoArray + index
                                    );
                        }
                    }
                }
                else
                {
                    // Memory error.
                    returnVal = -1;
                }
            }
            // else either failure (return -1) or 0-length array (return 0),
            // nothing special needs to be done.
            
            gdrive_json_kill(pObj);
        }
        // else do nothing.  Already prepared to return error.
    }
    
    _gdrive_download_buffer_free(pBuf);

    
    pArray->nItems = returnVal;
    return returnVal;
}

/*
 * The path argument should be an absolute path, starting with '/'. Can be 
 * either a file or a directory (folder).
 */
char* gdrive_filepath_to_id(Gdrive_Info* pInfo, const char* path)
{
    char* result = NULL;
    if (path == NULL || (path[0] != '/'))
    {
        // Invalid path
        return NULL;
    }
    
    // Treat an empty string as 
    
    // Try to get the ID from the cache.
    const char* cachedId = _gdrive_fileid_cache_get_item(pInfo, path);
    if (cachedId != NULL)
    {
        result = malloc(strlen(cachedId) + 1);
        if (result != NULL)
        {
            strcpy(result, cachedId);
        }
        return result;
    }
    // else ID isn't in the cache yet
    
    // Is this the root folder?
    if (strcmp(path, "/") == 0)
    {
        result = _gdrive_get_root_folder_id(pInfo);
        if (result != NULL)
        {
            // Add to the fileId cache.
            _gdrive_fileid_cache_add_item(
                &(pInfo->pInternalInfo->cache.fileIdCacheHead), 
                path, 
                result
                );
        }
        return result;
    }
    
    // Not in cache, and not the root folder.  Some part of the path may be
    // cached, and some part MUST be the root folder, so recursion seems like
    // the easiest solution here.
    
    // Find the last '/' character (ignoring any trailing slashes, which we
    // shouldn't get anyway). Everything before it is the parent, and everything
    // after is the child.
    int index;
    // Ignore trailing slashes
    for (index = strlen(path) - 1; path[index] == '/'; index--)
        ;   // No loop body
    // Find the last '/' before the current index.
    for (/*No init*/; path[index] != '/'; index--)
        ;   // No loop body
    
    // Find the parent's fileId.
    
    // Normally don't include the '/' at the end of the path, EXCEPT if we've
    // reached the start of the string. We expect to see "/" for the root
    // directory, not an empty string.
    int parentLength = (index != 0) ? index : 1;
    char* parentPath = malloc(parentLength + 1);
    if (parentPath == NULL)
    {
        // Memory error
        return NULL;
    }
    strncpy(parentPath, path, parentLength);
    parentPath[parentLength] = '\0';
    char* parentId = gdrive_filepath_to_id(pInfo, parentPath);
    free(parentPath);
    if (parentId == NULL)
    {
        // An error occurred.
        return NULL;
    }
    // Use the parent's ID to find the child's ID.
    result = _gdrive_get_child_id_by_name(pInfo, parentId, path + index + 1);
    free(parentId);
    
    // Add the ID to the fileId cache.
    if (result != NULL)
    {
        _gdrive_fileid_cache_add_item(
                &(pInfo->pInternalInfo->cache.fileIdCacheHead), 
                path, 
                result
                );
    }
    return result;
}

void gdrive_fileinfo_cleanup(Gdrive_Fileinfo* pFileinfo)
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

Gdrive_Fileinfo_Array* gdrive_fileinfo_array_create(void)
{
    Gdrive_Fileinfo_Array* pArray = malloc(sizeof(Gdrive_Fileinfo_Array));
    if (pArray != NULL)
    {
        pArray->nItems = 0;
        pArray->pArray = NULL;
    }
    return pArray;
}

void gdrive_fileinfo_array_free(Gdrive_Fileinfo_Array* pArray)
{
    for (int i = 0; i < pArray->nItems; i++)
    {
        gdrive_fileinfo_cleanup(pArray->pArray + i);
    }
    
    if (pArray->nItems > 0)
    {
        free(pArray->pArray);
    }
    
    // Not really necessary, but doesn't harm anything
    pArray->nItems = 0;
    pArray->pArray = NULL;
    
    free(pArray);
}


void gdrive_cleanup(Gdrive_Info* pInfo)
{
    gdrive_cleanup_nocurl(pInfo);
    curl_global_cleanup();
}

void gdrive_cleanup_nocurl(Gdrive_Info* pInfo)
{
    _gdrive_info_free(pInfo);
}


int gdrive_rfc3339_to_epoch_timens(const char* rfcTime, 
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

Gdrive_Filehandle* gdrive_file_open(Gdrive_Info* pInfo, 
                                    const char* fileId,
                                    int flags
)
{
    // Get the cache node from the cache if it exists.  If it doesn't exist,
    // don't make a node with an empty Gdrive_Fileinfo.  Instead, use 
    // gdrive_file_info_from_id() to create the node and fill out the struct, 
    // then try again to get the node.
    Gdrive_Cache_Node* pNode;
    while ((pNode = 
            _gdrive_cache_get_node(NULL, 
                                   &(pInfo->pInternalInfo->cache.pCacheHead),
                                   fileId, false, NULL)
            ) == NULL)
    {
        Gdrive_Fileinfo* pDummy;
        if (gdrive_file_info_from_id(pInfo, fileId, &pDummy) != 0)
        {
            // Problem getting the file info.  Return failure.
            return NULL;
        }
    }
    
    // Don't open directories, only regular files.
    if (pNode->fileinfo.type == GDRIVE_FILETYPE_FOLDER)
    {
        // Return failure
        return NULL;
    }
    
    // Increment open file counts.
    if ((flags & O_RDONLY) || (flags & O_RDWR))
    {
        // Open for reading (not necessarily only reading)
        pNode->openReads++;
    }
    if ((flags & O_WRONLY) || (flags & O_RDWR))
    {
        // Open for writing (not necessarily only writing)
        pNode->openWrites++;
    }
    
    // Return a pointer to the cache node (which is typedef'ed to 
    // Gdrive_Filehandle)
    return pNode;
    
}

void gdrive_file_close(Gdrive_Info* pInfo, Gdrive_Filehandle* pFile)
{
    Gdrive_Cache_Node* pNode = pFile;
    
    // TODO: This is a stub.
    (void) pInfo;
    (void) pFile;
    (void) pNode;
}

int gdrive_file_read(Gdrive_Info* pInfo, 
                     char* buf,
                     size_t size, 
                     off_t offset, 
                     Gdrive_Filehandle* pFile
)
{
    off_t nextOffset = offset;
    off_t bufferOffset = 0;
    size_t bytesRemaining = size;
    
    while (bytesRemaining > 0)
    {
        off_t bytesRead = _gdrive_file_read_next_chunk(pInfo, 
                                                      pFile,
                                                      buf + bufferOffset,
                                                      nextOffset, 
                                                      bytesRemaining
                );
        if (bytesRead < 0)
        {
            // Read error.  bytesRead is the negative error number
            return bytesRead;
        }
        if (bytesRead == 0)
        {
            // EOF. Return the total number of bytes actually read.
            return size - bytesRemaining;
        }
        nextOffset += bytesRead;
        bufferOffset += bytesRead;
        bytesRemaining -= bytesRead;
    }
    
    return size;
}






/*************************************************************************
 * Functions below this line are intended for internal use within gdrive.c
 *************************************************************************/

/*
 * _gdrive_info_create():    Creates and initializes a Gdrive_Info struct.
 * Parameters:
 *      ppInfo (Gdrive_Info**):
 *          If the value on input is the address of:
 *              A.  A NULL pointer (which it should normally be), then on output
 *                  the pointer will point to a newly allocated Gdrive_Info.
 *                  The pInternalInfo member will point to a newly allocated
 *                  struct, and all other members are initialized to 0.  If an
 *                  error prevents the creation of the Gdrive_Info struct, this
 *                  will remain a NULL pointer.  If the Gdrive_Info is created
 *                  but an error prevents Gdrive_Info_Internal creation, then
 *                  the pInternalInfo member will be initialized to NULL.
 *              B.  A non-NULL pointer to an existing Gdrive_Info that has a
 *                  NULL pInternalInfo member (which might occur if a previous
 *                  call to _gdrive_info_create() partially failed), then on 
 *                  output the pointer will continue pointing to the existing 
 *                  Gdrive_Info.  The pInternalInfo member will point to a newly
 *                  allocated struct, or remain NULL in the case of an error.
 *              C.  A non-NULL pointer to an existing Gdrive_Info with a 
 *                  non-NULL pInternalInfo member, no changes will be made.
 * Returns:
 *      0 on success, other value on failure.  Note, a Gdrive_Info struct may
 *      be allocated even if the function fails.
 */
int _gdrive_info_create(Gdrive_Info** ppInfo)
{
    // Create the Gdrive_Info struct and initialize everything to 0, unless
    // it already exists.
    if (*ppInfo == NULL)
    {
        if ((*ppInfo = malloc(sizeof(Gdrive_Info))) == NULL)
        {
            // Memory allocation failed, return error.  For now, return -1.
            // Later, we may define different error conditions.
            return -1;
        }
        memset(*ppInfo, 0, sizeof(Gdrive_Info));
    }
    
    // Create the Gdrive_Internal_Info struct, unless it already exists.
    if ((*ppInfo)->pInternalInfo == NULL)
    {
        return _gdrive_info_internal_create(&((*ppInfo)->pInternalInfo));
    }
    
    // If we've gotten this far, then nothing has been done, so nothing
    // has failed.
    return 0;
}

/*
 * _gdrive_info_internal_create():  Creates and initializes a 
 *                                  Gdrive_Info_Internal struct.
 * Parameters:
 *      ppInfo (Gdrive_Info_Internal**):
 *          If the value on input is the address of:
 *              A.  A NULL pointer (which it should normally be), then on output
 *                  the pointer will point to a newly allocated 
 *                  Gdrive_Info_Internal.  The members clientId, clientSecret,
 *                  and redirectUri will be set appropriately, and all members 
 *                  are initialized to 0.  If an error prevents the creation of 
 *                  the struct, this will remain a NULL pointer.
 *              B.  A non-NULL pointer to an existing Gdrive_Info_Internal, then
 *                  no changes will be made.
 * Returns:
 *      0 on success, other value on failure.
 */
int _gdrive_info_internal_create(Gdrive_Info_Internal** ppInfo)
{
    if (*ppInfo != NULL)
    {
        // Already exists, nothing to do, return success.
        return 0;
    }
    
    if ((*ppInfo = malloc(sizeof(Gdrive_Info_Internal))) == NULL)
    {
        // Memory allocation failed, return error.  For now, return -1.
        // Later, we may define different error conditions.
        return -1;
    }
    memset(*ppInfo, 0, sizeof(Gdrive_Info_Internal));
    (*ppInfo)->clientId = GDRIVE_CLIENT_ID;
    (*ppInfo)->clientSecret = GDRIVE_CLIENT_SECRET;
    (*ppInfo)->redirectUri = GDRIVE_REDIRECT_URI;
    
    // If we've gotten this far, we've succeeded.
    return 0;
}

int _gdrive_read_auth_file(const char* filename, Gdrive_Info_Internal* pInfo)
{
    if (filename == NULL || pInfo == NULL)
    {
        // Invalid argument.  For now, return -1 for all errors.
        return -1;
    }
    
    
    // Make sure the file exists and is a regular file.
    struct stat st;
    if ((stat(filename, &st) == 0) && (st.st_mode & S_IFREG))
    {
        FILE* inFile = fopen(filename, "r");
        if (inFile == NULL)
        {
            // Couldn't open file for reading.
            return -1;
        }
        
        char* buffer = malloc(st.st_size + 1);
        if (buffer == NULL)
        {
            // Memory allocation error.
            fclose(inFile);
            return -1;
        }
        
        int bytesRead = fread(buffer, 1, st.st_size, inFile);
        buffer[bytesRead>=0 ? bytesRead : 0] = '\0';
        int returnVal = 0;
        
        gdrive_json_object* pObj = gdrive_json_from_string(buffer);
        if (pObj == NULL)
        {
            // Couldn't convert the file contents to a JSON object, prepare to
            // return failure.
            returnVal = -1;
        }
        else
        {
            pInfo->accessToken = _gdrive_new_string_from_json(
                    pObj, 
                    GDRIVE_FIELDNAME_ACCESSTOKEN, 
                    &(pInfo->accessTokenLength)
                    );
            pInfo->refreshToken = _gdrive_new_string_from_json(
                    pObj, 
                    GDRIVE_FIELDNAME_REFRESHTOKEN, 
                    &(pInfo->refreshTokenLength)
                    );
            
            if ((pInfo->accessToken == NULL) || (pInfo->refreshToken == NULL))
            {
                // Didn't get one or more auth tokens from the file.
                returnVal = -1;
            }

        }
        free(buffer);
        fclose(inFile);
        return returnVal;

    }
    else
    {
        // File doesn't exist or isn't a regular file.
        return -1;
    }
    
}


void _gdrive_info_free(Gdrive_Info* pInfo)
{
    _gdrive_info_internal_free(pInfo->pInternalInfo);
    pInfo->pInternalInfo = NULL;
    
    _gdrive_settings_cleanup(&(pInfo->settings));
    
    free(pInfo);
}

void _gdrive_settings_cleanup(Gdrive_Settings* pSettings)
{
    // DO NOT FREE pSettings!!!!  Only free any internal members that were
    // malloc'ed.
    
    free(pSettings->authFilename);
    pSettings->authFilename = NULL;
}

void _gdrive_info_internal_free(Gdrive_Info_Internal* pInfo)
{
    // Setting the pointers to NULL after freeing them isn't necessary
    // because we will free the whole struct in the end, but it doesn't hurt
    // anything.
    free(pInfo->accessToken);
    pInfo->accessToken = NULL;
    pInfo->accessToken = 0;
    
    free(pInfo->refreshToken);
    pInfo->refreshToken = NULL;
    pInfo->refreshToken = 0;
    
    if (pInfo->curlHandle != NULL)
    {
        curl_easy_cleanup(pInfo->curlHandle);
        pInfo->curlHandle = NULL;
    }
    
    free(pInfo);
}

CURLcode _gdrive_download_to_buffer(CURL* curlHandle, 
                                    Gdrive_Download_Buffer* pBuffer,
                                    bool textMode
)
{
    // Make sure data gets written at the start of the buffer.
    pBuffer->usedSize = 0;
    
    // Accept compressed responses.
    curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, "");
    
    // Automatically follow redirects
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1);

    
    // Set the destination - either our own callback function to fill the
    // in-memory buffer, or the default libcurl function to write to a FILE*.
    if (pBuffer->fh == NULL)
    {
        curl_easy_setopt(curlHandle, 
                         CURLOPT_WRITEFUNCTION, 
                         (textMode ? 
                             _gdrive_download_buffer_callback_text : 
                             _gdrive_download_buffer_callback_bin)
                );
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, pBuffer);
    }
    else
    {
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, pBuffer->fh);
    }
    
    // Do the transfer.
    pBuffer->resultCode = curl_easy_perform(curlHandle);
    
    // Get the HTTP response
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &(pBuffer->httpResp));
    
    return pBuffer->resultCode;
}

size_t _gdrive_download_buffer_callback_text(char *newData, 
                                             size_t size, 
                                             size_t nmemb, 
                                             void *userdata
)
{
    return _gdrive_download_buffer_callback(newData, 
                                            size, 
                                            nmemb, 
                                            userdata, 
                                            true
            );
}

size_t _gdrive_download_buffer_callback_bin(char *newData, 
                                            size_t size, 
                                            size_t nmemb, 
                                            void *userdata
)
{
    return _gdrive_download_buffer_callback(newData, 
                                            size, 
                                            nmemb, 
                                            userdata, 
                                            false
            );
}

size_t _gdrive_download_buffer_callback(char *newData, 
                                        size_t size, 
                                        size_t nmemb, 
                                        void *userdata,
                                        bool textMode
)
{
    if (size == 0 || nmemb == 0)
    {
        // No data
        return 0;
    }
    if (textMode != 0)
    {
        textMode = 1;
    }
    
    Gdrive_Download_Buffer* pBuffer = (Gdrive_Download_Buffer*) userdata;
    
    // Find the length of the data, and allocate more memory if needed.  If
    // textMode is true, include an extra byte to explicitly null terminate
    // (doesn't hurt anything if it's already null terminated).
    size_t dataSize = size * nmemb;
    size_t totalSize = dataSize + pBuffer->usedSize + textMode;
    if (totalSize > pBuffer->allocatedSize)
    {
        // Allow extra room to reduce the number of realloc's.
        size_t allocSize = totalSize + dataSize;    
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
    if (textMode)
    {
        pBuffer->data[totalSize - 1] = '\0';
    }
    return dataSize;
}

Gdrive_Download_Buffer* _gdrive_download_buffer_create(size_t initialSize)
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
    pBuf->fh = NULL;
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

void _gdrive_download_buffer_free(Gdrive_Download_Buffer* pBuf)
{
    if (pBuf != NULL && pBuf->data != NULL && pBuf->allocatedSize > 0)
    {
        free(pBuf->data);
        pBuf->data = NULL;
    }
    free(pBuf);
}

char* _gdrive_postdata_assemble(CURL* curlHandle, int n, ...)
{
    char* result = NULL;
    
    if (n == 0)
    {
        // No arguments, nothing to do.
        return NULL;
    }
    
    // Array to hold the URL-encoded strings
    char** encodedStrings = malloc(2 * n * sizeof(char*));
    if (encodedStrings == NULL)
    {
        // Memory error
        return NULL;
    }
    
    // Bytes needed to store the result.
    size_t length = 0;
    
    // URL-encode each of the arguments.
    va_list args;
    va_start(args, n);
    for (int i = 0; i < 2 * n; i++)
    {
        const char* arg = va_arg(args, const char*);
        encodedStrings[i] = curl_easy_escape(curlHandle, arg, 0);
        
        // Need strlen() + 1 bytes.  The extra byte could be '=' (after a field
        // name), '&' (after all but the last value), or terminating null (after
        // the final value), but it's always one extra character.
        length += strlen(encodedStrings[i]) + 1;
    }
    
    result = malloc(length);
    if (result == NULL)
    {
        // Memory error.  Cleanup and return NULL.
        for (int i = 0; i < 2 * n; i++)
        {
            curl_free(encodedStrings[i]);
        }
        free(encodedStrings);
        return NULL;
    }
    
    // Start with an empty string, then add the URL-encoded field names and 
    // values.
    result[0] = '\0';
    for (int i = 0; i < n; i++)
    {
        // Add the field name and "=".
        strcat(result, encodedStrings[2 * i]);
        strcat(result, "=");
        
        // Add the value and (if applicable) "&".
        strcat(result, encodedStrings[2 * i + 1]);
        if (i < n - 1)
        {
            strcat(result, "&");
        }
    }
    
    // Clean up the encoded strings
    for (int i = 0; i < 2 * n; i++)
    {
        curl_free(encodedStrings[i]);
    }
    free(encodedStrings);
    
    return result;
}

char* _gdrive_assemble_query_string(CURL* curlHandle, 
                                    const char* url, 
                                    int n, 
                                    ...
)
{
    // It would be nice to just reuse _gdrive_postdata_assemble(), then strcat()
    // the result onto the url string, but I don't know how to do that with
    // varargs.
    
    
    char* result = NULL;
    
    if (n == 0)
    {
        // No arguments, just copy the original url.
        result = malloc(strlen(url) + 1);
        if (result != NULL)
        {
            strcpy(result, url);
        }
        return result;
    }
    
    // Array to hold the URL-encoded strings
    char** encodedStrings = malloc(2 * n * sizeof(char*));
    if (encodedStrings == NULL)
    {
        // Memory error
        return NULL;
    }
    
    // Bytes needed to store the result.  Start with the length of the original
    // URL string, plus the '?' (not including the terminating null)
    size_t length = strlen(url) + 1;
    
    // URL-encode each of the arguments.
    va_list args;
    va_start(args, n);
    for (int i = 0; i < 2 * n; i++)
    {
        const char* arg = va_arg(args, const char*);
        encodedStrings[i] = curl_easy_escape(curlHandle, arg, 0);
        
        // Need strlen() + 1 bytes.  The extra byte could be '=' (after a field
        // name), '&' (after all but the last value), or terminating null (after
        // the final value), but it's always one extra character.
        length += strlen(encodedStrings[i]) + 1;
    }
    
    result = malloc(length);
    if (result == NULL)
    {
        // Memory error
        for (int i = 0; i < 2 * n; i++)
        {
            curl_free(encodedStrings[i]);
        }
        free(encodedStrings);
        return NULL;
    }
    
    // Copy the original URL and append "?".
    strcpy(result, url);
    strcat(result, "?");
    
    for (int i = 0; i < n; i++)
    {
        // Add the field name and "=".
        strcat(result, encodedStrings[2 * i]);
        strcat(result, "=");
        
        // Add the value and (if applicable) "&".
        strcat(result, encodedStrings[2 * i + 1]);
        if (i < n - 1)
        {
            strcat(result, "&");
        }
    }
    
    // Clean up the encoded strings
    for (int i = 0; i < 2 * n; i++)
    {
        curl_free(encodedStrings[i]);
    }
    free(encodedStrings);
    
    return result;
}

int _gdrive_refresh_auth_token(Gdrive_Info* pInfo, 
                               //Gdrive_Download_Buffer* pBuf,
                               const char* grantType,
                               const char* tokenString
)
{
    // Make sure we were given a valid grant_type
    if (strcmp(grantType, GDRIVE_GRANTTYPE_CODE) && 
            strcmp(grantType, GDRIVE_GRANTTYPE_REFRESH))
    {
        // Invalid grant_type
        return -1;
    }
    
    CURL* curlHandle = pInfo->pInternalInfo->curlHandle;
    
    if (curlHandle == NULL)
    {
        if ((curlHandle = curl_easy_init()) == NULL)
        {
            // Couldn't get a curl easy handle, return error.
            return -1;
        }
    }

    Gdrive_Query* pPostData = _gdrive_query_create(curlHandle);
    if (pPostData == NULL)
    {
        // Memory error
        return -1;
    }
    const char* tokenOrCodeField = NULL;
    if (strcmp(grantType, GDRIVE_GRANTTYPE_CODE) == 0)
    {
        // Converting an auth code into auth and refresh tokens.  Interpret
        // tokenString as the auth code.
        _gdrive_query_add(pPostData,
                          GDRIVE_FIELDNAME_REDIRECTURI, 
                          GDRIVE_REDIRECT_URI
                );
        tokenOrCodeField = GDRIVE_FIELDNAME_CODE;
    }
    else
    {
        // Refreshing an existing refresh token.  Interpret tokenString as the
        // refresh token.
        tokenOrCodeField = GDRIVE_FIELDNAME_REFRESHTOKEN;
    }
    _gdrive_query_add(pPostData, tokenOrCodeField, tokenString);
    _gdrive_query_add(pPostData, GDRIVE_FIELDNAME_CLIENTID, GDRIVE_CLIENT_ID);
    _gdrive_query_add(pPostData,
                      GDRIVE_FIELDNAME_CLIENTSECRET, 
                      GDRIVE_CLIENT_SECRET
            );
    _gdrive_query_add(pPostData, GDRIVE_FIELDNAME_GRANTTYPE, grantType);
        
    // Do the transfer. We're trying to get authorization, so don't retry on
    // auth errors.
    Gdrive_Download_Buffer* pBuf = 
            _gdrive_do_transfer(pInfo, GDRIVE_REQUEST_POST, false, 
                                GDRIVE_URL_AUTH_TOKEN, pPostData, NULL, NULL
            );
    _gdrive_query_free(pPostData);
    
    if (pBuf == NULL)
    {
        // There was an error sending the request and getting the response.
        return -1;
    }
    if (pBuf->httpResp >= 400)
    {
        // Failure, but probably not an error.  Most likely, the user has
        // revoked permission or the refresh token has otherwise been
        // invalidated.
        _gdrive_download_buffer_free(pBuf);
        return 1;
    }
    
    // If we've gotten this far, we have a good HTTP response.  Now we just
    // need to pull the access_token string (and refresh token string if
    // present) out of it.

    gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
    _gdrive_download_buffer_free(pBuf);
    if (pObj == NULL)
    {
        // Couldn't locate JSON-formatted information in the server's 
        // response.  Return error.
        return -1;
    }
    int returnVal = _gdrive_realloc_string_from_json(
            pObj, 
            GDRIVE_FIELDNAME_ACCESSTOKEN,
            &(pInfo->pInternalInfo->accessToken),
            &(pInfo->pInternalInfo->accessTokenLength)
            );
    // Only try to get refresh token if we successfully got the access 
    // token.
    if (returnVal == 0)
    {
        // We won't always have a refresh token.  Specifically, if we were
        // already sending a refresh token, we may not get one back.
        // Don't treat the lack of a refresh token as an error or a failure,
        // and don't clobber the existing refresh token if we don't get a
        // new one.

        long length = gdrive_json_get_string(pObj, 
                                        GDRIVE_FIELDNAME_REFRESHTOKEN, 
                                        NULL, 0
                );
        if (length < 0 && length != INT64_MIN)
        {
            // We were given a refresh token, so store it.
            _gdrive_realloc_string_from_json(
                    pObj, 
                    GDRIVE_FIELDNAME_REFRESHTOKEN,
                    &(pInfo->pInternalInfo->refreshToken),
                    &(pInfo->pInternalInfo->refreshTokenLength)
                    );
        }
    }
    gdrive_json_kill(pObj);
    
    return returnVal;
}

int _gdrive_prompt_for_auth(Gdrive_Info* pInfo)
{
    char scopeStr[GDRIVE_SCOPE_MAXLENGTH] = "";
    bool scopeFound = false;
    
    // Check each of the possible permissions, and add the appropriate scope
    // if necessary.
    for (int i = 0; i < GDRIVE_ACCESS_MODE_COUNT; i++)
    {
        if (pInfo->settings.mode & GDRIVE_ACCESS_MODES[i])
        {
            // If this isn't the first scope, add a space to separate scopes.
            if (scopeFound)
            {
                strcat(scopeStr, " ");
            }
            // Add the current scope.
            strcat(scopeStr, GDRIVE_ACCESS_SCOPES[i]);
            scopeFound = true;
        }
    }
    
    char* authUrl = _gdrive_assemble_query_string(pInfo->pInternalInfo->curlHandle,
                                                  GDRIVE_URL_AUTH_NEWAUTH,
                                                  5,
                                                  "response_type", "code",
                                                  "client_id", GDRIVE_CLIENT_ID,
                                                  "redirect_uri", 
                                                  GDRIVE_REDIRECT_URI,
                                                  "scope", scopeStr,
                                                  "include_granted_scopes", 
                                                  "true"
    );
    
    if (authUrl == NULL)
    {
        // Return error
        return -1;
    }
    
    // Prompt the user.
    puts("This program needs access to a Google Drive account.\n"
            "To grant access, open the following URL in your web\n"
            "browser.  Copy the code that you receive, and paste it\n"
            "below.\n\n"
            "The URL to open is:");
    puts(authUrl);
    puts("\nPlease paste the authorization code here:");
    free(authUrl);
    
    // The authorization code should never be this long, so it's fine to ignore
    // longer input
    char authCode[1024] = "";
    fgets(authCode, 1024, stdin);
    
    if (authCode[0] == '\0')
    {
        // No code entered, return failure.
        return 1;
    }
    
    // Exchange the authorization code for access and refresh tokens.
    return _gdrive_refresh_auth_token(pInfo, GDRIVE_GRANTTYPE_CODE, authCode);
}

int _gdrive_check_scopes(Gdrive_Info* pInfo)
{
    // Convenience assignments
    Gdrive_Info_Internal* pInternalInfo = pInfo->pInternalInfo;
    CURL* curlHandle = pInternalInfo->curlHandle;
    
    Gdrive_Query* pQuery = _gdrive_query_create(curlHandle);
    if (pQuery == NULL)
    {
        // Memory error
        return -1;
    }
    _gdrive_query_add(pQuery, 
                      GDRIVE_FIELDNAME_ACCESSTOKEN, 
                      pInternalInfo->accessToken
            );
    
    Gdrive_Download_Buffer* pBuf = 
            _gdrive_do_transfer(pInfo, GDRIVE_REQUEST_GET, false, 
                                GDRIVE_URL_AUTH_TOKENINFO, pQuery, NULL, NULL
            );
    _gdrive_query_free(pQuery);
    
    if (pBuf == NULL || pBuf->httpResp >= 400)
    {
        // Download failed or gave a bad response.
        _gdrive_download_buffer_free(pBuf);
        return -1;
    }
    
    // If we've made it this far, we have an ok response.  Extract the scopes
    // from the JSON array that should have been returned, and compare them
    // with the expected scopes.
    
    gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
    _gdrive_download_buffer_free(pBuf);
    if (pObj == NULL)
    {
        // Couldn't interpret the response as JSON, return error.
        return -1;
    }
    char* grantedScopes = _gdrive_new_string_from_json(pObj, "scope", NULL);
    if (grantedScopes == NULL)
    {
        // Key not found, or value not a string.  Return error.
        gdrive_json_kill(pObj);
        return -1;
    }
    gdrive_json_kill(pObj);
    
    
    // Go through each of the space-separated scopes in the string, comparing
    // each one to the GDRIVE_ACCESS_SCOPES array.
    long startIndex = 0;
    long endIndex = 0;
    int matchedScopes = 0;
    while (grantedScopes[startIndex] != '\0')
    {
        // After the loop executes, startIndex indicates the start of a scope,
        // and endIndex indicates the (null or space) terminator character.
        for (
                endIndex = startIndex; 
                !(grantedScopes[endIndex] == ' ' || grantedScopes[endIndex] == '\0');
                endIndex++
                );  // No loop body
        
        // Compare the current scope to each of the entries in 
        // GDRIVE_ACCESS_SCOPES.  If there's a match, set the appropriate bit(s)
        // in matchedScopes.
        for (int i = 0; i < GDRIVE_ACCESS_MODE_COUNT; i++)
        {
            if (strncmp(GDRIVE_ACCESS_SCOPES[i], 
                        grantedScopes + startIndex, 
                        endIndex - startIndex
                    ) == 0)
            {
                matchedScopes = matchedScopes | GDRIVE_ACCESS_MODES[i];
            }
        }
        
        startIndex = endIndex + 1;
    }
    free(grantedScopes);
    
    // Compare the access mode we encountered to the one we expected, one piece
    // at a time.  If we don't find what we need, return failure.
    for (int i = 0; i < GDRIVE_ACCESS_MODE_COUNT; i++)
    {
        if ((pInfo->settings.mode & GDRIVE_ACCESS_MODES[i]) && 
                !(matchedScopes & GDRIVE_ACCESS_MODES[i])
                )
        {
            return -1;
        }
    }
    
    // If we made it through to here, return success.
    return 0;
}

char* _gdrive_get_root_folder_id(Gdrive_Info* pInfo)
{
    // String to hold the url
    char* url = malloc(strlen(GDRIVE_URL_FILES) + 
                        strlen("/root?fields=id") + 1);
    if (url == NULL)
    {
        // Memory error.
        return NULL;
    }
    strcpy(url, GDRIVE_URL_FILES);
    strcat(url, "/root?fields=id");
    
    Gdrive_Download_Buffer* pBuf = _gdrive_do_transfer(pInfo, 
                                                       GDRIVE_REQUEST_GET, true,
                                                       url, NULL, NULL, NULL
            );
    free(url);
    
    if (pBuf == NULL || pBuf->httpResp >= 400)
    {
        // Download error or bad response
        _gdrive_download_buffer_free(pBuf);
        return NULL;
    }
    
    // If we're here, we have a good response.  Extract the ID from the 
    // response.
    
    // Convert to a JSON object.
    gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
    _gdrive_download_buffer_free(pBuf);
    if (pObj == NULL)
    {
        // Couldn't convert to JSON object.
        return NULL;
    }
    
    char* id = _gdrive_new_string_from_json(pObj, "id", NULL);
    return id;
}

char* _gdrive_get_child_id_by_name(Gdrive_Info* pInfo, 
                                   const char* parentId, 
                                   const char* childName
)
{
    // Construct a filter in the form of 
    // "'<parentId>' in parents and title = '<childName>'"
    char* filter = malloc(strlen("'' in parents and title = ''") + 
                          strlen(parentId) + strlen(childName) + 1
    );
    if (filter == NULL)
    {
        // Memory error
        return NULL;
    }
    strcpy(filter, "'");
    strcat(filter, parentId);
    strcat(filter, "' in parents and title = '");
    strcat(filter, childName);
    strcat(filter, "'");
    
    CURL* curlHandle = pInfo->pInternalInfo->curlHandle;

    Gdrive_Query* pQuery = _gdrive_query_create(curlHandle);
    if (pQuery == NULL)
    {
        // Memory error
        free(filter);
        return NULL;
    }
    _gdrive_query_add(pQuery, "q", filter);
    _gdrive_query_add(pQuery, "fields", "items(id)");
    

    Gdrive_Download_Buffer* pBuf = 
            _gdrive_do_transfer(pInfo, GDRIVE_REQUEST_GET, true, 
                                GDRIVE_URL_FILES, pQuery, NULL, NULL
            );
    _gdrive_query_free(pQuery);
    
    if (pBuf == NULL || pBuf->httpResp >= 400)
    {
        // Download error
        _gdrive_download_buffer_free(pBuf);
        return NULL;
    }
    
    
    // If we're here, we have a good response.  Extract the ID from the 
    // response.
    
    // Convert to a JSON object.
    gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
    _gdrive_download_buffer_free(pBuf);
    if (pObj == NULL)
    {
        // Couldn't convert to JSON object.
        return NULL;
    }
    
    char* id = NULL;
    gdrive_json_object* pArrayItem = gdrive_json_array_get(pObj, "items", 0);
    if (pArrayItem != NULL)
    {
        id = _gdrive_new_string_from_json(pArrayItem, "id", NULL);
    }
    return id;
}

size_t _gdrive_file_read_next_chunk(Gdrive_Info* pInfo, 
                                    Gdrive_Cache_Node* pNode,
                                    char* destBuf, 
                                    off_t offset, 
                                    size_t size
)
{
    // Do we already have a chunk that includes the starting point?
    FILE* fileChunk = 
            _gdrive_file_contents_find_chunk(pNode->pContents, offset);
    
    if (fileChunk == NULL)
    {
        // Chunk doesn't exist, need to create and download it.
        fileChunk = _gdrive_file_contents_create_chunk(pInfo, 
                                                       pNode, 
                                                       offset, 
                                                       size
                );
        
        if (fileChunk == NULL)
        {
            // Error creating the chunk
            return -EIO;
        }
    }
    
    // Read the data into the supplied buffer.
    fseek(fileChunk, offset, SEEK_SET);
    size_t bytesRead = fread(destBuf, 1, size, fileChunk);
    
    // If an error occurred, return negative.
    if (bytesRead < size)
    {
        int err = ferror(fileChunk);
        if (err != 0)
        {
            rewind(fileChunk);
            return -err;
        }
    }
    
    // Return the number of bytes read (which may be less than size if we hit
    // EOF).
    return bytesRead;
    
}

FILE* _gdrive_file_contents_find_chunk(Gdrive_File_Contents* pContents, 
                                       off_t offset
)
{
    if (pContents == NULL || pContents->fh == NULL)
    {
        // Nothing here, return failure.
        return NULL;
    }
    
    if (offset > pContents->start && offset < pContents->end)
    {
        // Found it!
        return pContents->fh;
    }
    
    // It's not at this node.  Try the next one.
    return _gdrive_file_contents_find_chunk(pContents->pNext, offset);
}

FILE* _gdrive_file_contents_create_chunk(Gdrive_Info* pInfo, 
                                         Gdrive_Cache_Node* pNode, 
                                         off_t offset,
                                         size_t size
)
{
    // Get the normal chunk size for this file, the smallest multiple of
    // minChunkSize that results in maxChunks or fewer chunks.
    size_t fileSize = pNode->fileinfo.size;
    int maxChunks = pInfo->settings.maxChunks;
    size_t minChunkSize = pInfo->settings.minChunkSize;

    size_t perfectChunkSize = _gdrive_divide_round_up(fileSize, maxChunks);
    size_t chunkSize = _gdrive_divide_round_up(perfectChunkSize, minChunkSize) *
            minChunkSize;
    
    // The actual chunk may be a multiple of chunkSize.  A read that starts at
    // "offset" and is "size" bytes long should be within this single chunk.
    off_t chunkStart = (offset / chunkSize) * chunkSize;
    off_t chunkOffset = offset % chunkSize;
    off_t endChunkOffset = chunkOffset + size - 1;
    size_t realChunkSize = _gdrive_divide_round_up(endChunkOffset, chunkSize) *
            chunkSize;
    
    Gdrive_File_Contents* pContents = _gdrive_file_contents_create(pNode);
    if (pContents == NULL)
    {
        // Memory or file creation error
        return NULL;
    }
    
    int success = _gdrive_file_contents_fill_chunk(pInfo, 
                                                   &(pNode->fileinfo),
                                                   pContents, 
                                                   chunkStart, 
                                                   realChunkSize
            );
    if (success != 0)
    {
        // Didn't write the file.  Clean up the new Gdrive_File_Contents struct
        _gdrive_file_contents_free(pNode, pContents);
        return NULL;
    }
    
    //Success
    pContents->start = chunkStart;
    pContents->end = chunkStart + realChunkSize - 1;
    return (success == 0) ? pContents->fh : NULL;
}

Gdrive_File_Contents* _gdrive_file_contents_create(Gdrive_Cache_Node* pNode)
{
    // Find the last entry in the file contents list, and add a new one to the
    // end.
    Gdrive_File_Contents** ppContents = &(pNode->pContents);
    while (*ppContents != NULL)
    {
        ppContents = &((*ppContents)->pNext);
    }
    *ppContents = malloc(sizeof(Gdrive_File_Contents));
    
    // Convenience assignment
    Gdrive_File_Contents* pContents = *ppContents;
    
    if (pContents != NULL)
    {
        memset(pContents, 0, sizeof(Gdrive_File_Contents));
        
        // Create a temporary file on disk.  This will automatically be deleted
        // when the file is closed or when this program terminates, so no 
        // cleanup is needed.
        pContents->fh = tmpfile();
        if (pContents->fh == NULL)
        {
            // File creation error
            free(pContents);
            *ppContents = NULL;
            return NULL;
        }
    }
    return pContents;
}

void _gdrive_file_contents_free(Gdrive_Cache_Node* pNode, 
                                Gdrive_File_Contents* pContents
)
{
    // Find the pointer leading to pContents.
    Gdrive_File_Contents** ppContents = &(pNode->pContents);
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

int _gdrive_file_contents_fill_chunk(Gdrive_Info* pInfo,
                                     Gdrive_Fileinfo* pFileinfo, 
                                     Gdrive_File_Contents* pContents,
                                     off_t start, 
                                     size_t size
)
{
    // Construct the base URL in the form of "<GDRIVE_URL_FILES>/<fileId>".
    char* fileUrl = malloc(strlen(GDRIVE_URL_FILES) + 
                           strlen(pFileinfo->id) + 2
    );
    if (fileUrl == NULL)
    {
        // Memory error
        return -1;
    }
    strcpy(fileUrl, GDRIVE_URL_FILES);
    strcat(fileUrl, "/");
    strcat(fileUrl, pFileinfo->id);
    
    // Construct query parameters
    Gdrive_Query* pQuery = 
            _gdrive_query_create(pInfo->pInternalInfo->curlHandle);
    if (pQuery == NULL)
    {
        // Memory error
        free(fileUrl);
        return -1;
    }
    _gdrive_query_add(pQuery, "updateViewedDate", "true");
    _gdrive_query_add(pQuery, "alt", "media");
    
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
        _gdrive_query_free(pQuery);
        free(fileUrl);
        return -1;
    }
    snprintf(rangeHeader, rangeSize, "Range: bytes=%ld-%ld", start, end);
    struct curl_slist* pHeaders = curl_slist_append(NULL, rangeHeader);
    free(rangeHeader);
    if (pHeaders == NULL)
    {
        // An error occurred
        _gdrive_query_free(pQuery);
        free(fileUrl);
        return -1;
    }
    
    // Make sure the file position is at the start and any stream errors are
    // cleared (this should be redundant, since we should normally have a newly
    // created and opened temporary file).
    rewind(pContents->fh);
    
    // Perform the transfer
    Gdrive_Download_Buffer* pBuf = _gdrive_do_transfer(pInfo, 
                                                       GDRIVE_REQUEST_GET, true,
                                                       fileUrl, pQuery, 
                                                       pHeaders, pContents->fh
    );
    
    bool success = (pBuf != NULL && 
            pBuf->resultCode == CURLE_OK && 
            pBuf->httpResp < 400
            );
    _gdrive_download_buffer_free(pBuf);
    _gdrive_query_free(pQuery);
    free(fileUrl);
    return success ? 0 : -1;
}

/*
 * pHeaders can be NULL, or an existing set of headers can be given.
 */
struct curl_slist* _gdrive_authbearer_header(
        Gdrive_Info_Internal* pInternalInfo, struct curl_slist* pHeaders
)
{
    // First form a string with the required text and the access token.
    char* header = malloc(strlen("Authorization: Bearer ") + 
                          strlen(pInternalInfo->accessToken) + 1
    );
    if (header == NULL)
    {
        // Memory error
        return NULL;
    }
    strcpy(header, "Authorization: Bearer ");
    strcat(header, pInternalInfo->accessToken);
    
    // Copy the string into a curl_slist for use in headers.
    struct curl_slist* returnVal = curl_slist_append(pHeaders, header);
    return returnVal;
}

enum Gdrive_Retry_Method _gdrive_retry_on_error(Gdrive_Download_Buffer* pBuf, 
                                                long httpResp
)
{
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
        gdrive_json_object* pRoot = gdrive_json_from_string(pBuf->data);
        if (pRoot != NULL)
        {
            gdrive_json_object* pErrors = 
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

/*
 * pHeaders can (and usually should) be NULL. If given a non-NULL pHeaders,
 * then the calling function should NOT make any further use of this pointer,
 * and should NOT call curl_slist_free_all().
 */
Gdrive_Download_Buffer* _gdrive_do_transfer(
        Gdrive_Info* pInfo, enum Gdrive_Request_Type requestType,
        bool retryOnAuthError, const char* url,  const Gdrive_Query* pQuery, 
        struct curl_slist* pHeaders, FILE* destFile
)
{
    // Convenience assignment
    CURL* curlHandle = pInfo->pInternalInfo->curlHandle;
    
    // Get the Authorization: Bearer header
    struct curl_slist* pNewHeaders;
    pNewHeaders = _gdrive_authbearer_header(pInfo->pInternalInfo, pHeaders);
    if (pNewHeaders == NULL)
    {
        // Unknown error, possibly memory
        return NULL;
    }
    
    char* fullUrl = NULL;
    char* postData = NULL;
    
    switch (requestType)
    {
    case GDRIVE_REQUEST_GET:
        fullUrl = _gdrive_assemble_query_or_post(url, pQuery);
        if (fullUrl == NULL)
        {
            // Memory error or invalid URL
            curl_slist_free_all(pNewHeaders);
            return NULL;
        }
        curl_easy_setopt(curlHandle, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(curlHandle, CURLOPT_URL, fullUrl);
        // postData remains NULL and can be safely freed without harming 
        // anything.
        break;
        
    case GDRIVE_REQUEST_POST:
        postData = _gdrive_assemble_query_or_post(NULL, pQuery);
        if (postData == NULL)
        {
            // Memory error or invalid query
            curl_slist_free_all(pNewHeaders);
            return NULL;
        }
        curl_easy_setopt(curlHandle, CURLOPT_POST, 1);
        curl_easy_setopt(curlHandle, CURLOPT_URL, url);
        curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, postData);
        // fullUrl remains NULL and can be safely freed without harming
        // anything.
        break;
        
    default:
        // Unsupported request type.  PATCH should be added later.
        curl_slist_free_all(pNewHeaders);
        return NULL;
    }
    
    // Set headers
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, pNewHeaders);
    
    
    Gdrive_Download_Buffer* pBuf;
    pBuf = _gdrive_download_buffer_create((destFile == NULL) ? 512 : 0);
    if (pBuf == NULL)
    {
        // Memory error.
        curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, NULL);
        curl_slist_free_all(pNewHeaders);
        free(fullUrl);
        free(postData);
        return NULL;
    }
    pBuf->fh = destFile;
    
    int result = _gdrive_download_to_buffer_with_retry(pInfo, 
                                                       pBuf, 
                                                       retryOnAuthError, 
                                                       0, GDRIVE_RETRY_LIMIT
    );
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, NULL);
    curl_slist_free_all(pNewHeaders);
    free(fullUrl);
    free(postData);
    
    if (result != 0)
    {
        // Download failure
        _gdrive_download_buffer_free(pBuf);
        return NULL;
    }
    
    // The HTTP Response may be success (i.e., 200) or failure (400 or higher),
    // but the actual request succeeded as far as libcurl is concerned.  Return
    // the buffer.
    return pBuf;
}

int _gdrive_download_to_buffer_with_retry(Gdrive_Info* pInfo, 
                                          Gdrive_Download_Buffer* pBuf, 
                                          bool retryOnAuthError,
                                          int tryNum,
                                          int maxTries
)
{
    CURL* curlHandle = pInfo->pInternalInfo->curlHandle;
    
    CURLcode curlResult = _gdrive_download_to_buffer(curlHandle, pBuf, true);
    
    if (curlResult != CURLE_OK)
    {
        // Download error
        pBuf->httpResp = 0;
        return -1;
    }
    if (pBuf->httpResp >= 400)
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
        switch (_gdrive_retry_on_error(pBuf, pBuf->httpResp))
        {
        case GDRIVE_RETRY_RETRY:
            // Normal retry, use exponential backoff.
            _gdrive_exponential_wait(tryNum);
            retry = true;
            break;

        case GDRIVE_RETRY_RENEWAUTH:
            // Authentication error, probably expired access token.
            // If retryOnAuthError is true, refresh auth and retry (unless auth 
            // fails).
            if (retryOnAuthError)
            {
                retry = (gdrive_auth(pInfo) == 0);
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
            return _gdrive_download_to_buffer_with_retry(pInfo,
                                                         pBuf, 
                                                         //pHttpResp, 
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

void _gdrive_exponential_wait(int tryNum)
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

Gdrive_Query* _gdrive_query_create(CURL* curlHandle)
{
    Gdrive_Query* result = malloc(sizeof(Gdrive_Query));
    if (result != NULL)
    {
        memset(result, 0, sizeof(Gdrive_Query));
        result->curlHandle = curlHandle;
    }
    return result;
}

int _gdrive_query_add(Gdrive_Query* pQuery, 
                      const char* field, 
                      const char* value
)
{
    CURL* curlHandle = pQuery->curlHandle;
    
    Gdrive_Query* pLast = pQuery;
    if (pLast->field != NULL || pLast->value != NULL)
    {
        while (pLast->pNext != NULL)
        {
            pLast = pLast->pNext;
        }
        pLast->pNext = _gdrive_query_create(curlHandle);
        if (pLast->pNext == NULL)
        {
            // Memory error
            return -1;
        }
        pLast = pLast->pNext;
    }
    
    pLast->field = curl_easy_escape(curlHandle, field, 0);
    if (pLast->field == NULL)
    {
        // Error
        return -1;
    }
    pLast->value = curl_easy_escape(curlHandle, value, 0);
    if (pLast->value == NULL)
    {
        // Error
        return -1;
    }
    return 0;
}

void _gdrive_query_free(Gdrive_Query* pQuery)
{
    // Work from the end back to the beginning with recursion.
    if (pQuery->pNext != NULL)
    {
        _gdrive_query_free(pQuery->pNext);
    }
    
    // Free up the strings.  They were created with curl_easy_escape(), so we 
    // need to use curl_free().
    if (pQuery->field != NULL)
    {
        curl_free(pQuery->field);
    }
    if (pQuery->value != NULL)
    {
        curl_free(pQuery->value);
    }
    
    // Free the struct itself.
    free(pQuery);
}

char* _gdrive_assemble_query_or_post(const char* url, 
                                     const Gdrive_Query* pQuery
)
{
    // If there is a url, allow for its length plus the '?' character (or the
    // url length plus terminating null if there is no query string).
    int totalLength = (url == NULL) ? 0 : (strlen(url) +1);
    
    // If there is a query string (or POST data, which is handled the same way),
    // each field adds its length plus 1 for the '=' character. Each value adds
    // its length plus 1, for either the '&' character (all but the last item)
    // or the terminating null (on the last item).
    const Gdrive_Query* pCurrentQuery = pQuery;
    while (pCurrentQuery != NULL)
    {
        totalLength += strlen(pCurrentQuery->field) + 1;
        totalLength += strlen(pCurrentQuery->value) + 1;
        pCurrentQuery = pCurrentQuery->pNext;
    }
    
    if (totalLength < 1)
    {
        // Invalid arguments
        return NULL;
    }
    
    // Allocate a string long enough to hold everything.
    char* result = malloc(totalLength);
    if (result == NULL)
    {
        // Memory error
        return NULL;
    }
    
    // Copy the url into the result string.  If there is no url, start with an
    // empty string.
    if (url != NULL)
    {
        strcpy(result, url);
        if (pQuery == NULL)
        {
            // We had a URL string but no query string, so we're done.
            return result;
        }
        else
        {
            // We have both a URL and a query, so they need to be separated
            // with a '?'.
            strcat(result, "?");
        }
    }
    else
    {
        result[0] = '\0';
    }
    
    if (pQuery == NULL)
    {
        // There was no query string, so we're done.
        return result;
    }
    
    // Copy each of the query field/value pairs into the result.
    pCurrentQuery = pQuery;
    while (pCurrentQuery != NULL)
    {
        strcat(result, pCurrentQuery->field);
        strcat(result, "=");
        strcat(result, pCurrentQuery->value);
        if (pCurrentQuery->pNext != NULL)
        {
            strcat(result, "&");
        }
        pCurrentQuery = pCurrentQuery->pNext;
    }
    
    return result;
}

int _gdrive_realloc_string_from_json(gdrive_json_object* pObj, 
                                     const char* key,
                                     char** pDest, 
                                     long* pLength
)
{
    // Find the length of the string.
    long length = gdrive_json_get_string(pObj, key, NULL, 0);
    if (length == INT64_MIN || length == 0)
    {
        // Not a string (length includes the null terminator, so it can't
        // be 0).
        return -1;
    }

    length *= -1;
    // Make sure we have enough space to store the retrieved string.
    // If not, make more space with realloc.
    if (*pLength < length)
    {
        *pDest = realloc(*pDest, length);
        *pLength = length;
        if (*pDest == NULL)
        {
            // Memory allocation error
            *pLength = 0;
            return -1;
        }
    }

    // Actually retrieve the string.  This should return a non-zero positive
    // number, so determine success or failure based on that condition.
    return (gdrive_json_get_string(pObj, key, *pDest, length) > 0) ?
        0 :
        -1;
}

void _gdrive_get_fileinfo_from_json(Gdrive_Info* pInfo,
                                    gdrive_json_object* pObj, 
                                   Gdrive_Fileinfo* pFileinfo
)
{
    pFileinfo->filename = _gdrive_new_string_from_json(pObj, "title", NULL);
    pFileinfo->id = _gdrive_new_string_from_json(pObj, "id", NULL);
    bool success;
    pFileinfo->size = gdrive_json_get_int64(pObj, "fileSize", true, &success);
    if (!success)
    {
        pFileinfo->size = 0;
    }
    
    char* mimeType = _gdrive_new_string_from_json(pObj, "mimeType", NULL);
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
    // Get permissions based on filesystem mount options.
    int mountPerm = 0;
    if (pInfo->settings.mode & GDRIVE_ACCESS_WRITE)
    {
        // Full read-write access
        mountPerm = S_IWOTH | S_IROTH;
    }
    else if (pInfo->settings.mode & GDRIVE_ACCESS_READ)
    {
        // Read-only access
        mountPerm = S_IROTH;
    }
    // else no access (at least for regular files - folders are handled down
    // below)
    
    // Get the user's permissions for the file on the Google Drive account.
    char* role = _gdrive_new_string_from_json(pObj, "userPermission/role", NULL);
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
        
        // Find the intersection of the two sets of permissions.
        pFileinfo->permissions = mountPerm & basePerm;
        
        // Directories need read and execute permissions to be navigable.  Even
        // if the system has only meta-info permissions, add read/execute to
        // any folder.
        if (pFileinfo->type == GDRIVE_FILETYPE_FOLDER)
        {
            pFileinfo->permissions = pFileinfo->permissions | S_IROTH | S_IXOTH;
        }
        free(role);
    }
    
    char* cTime = _gdrive_new_string_from_json(pObj, "createdDate", NULL);
    if (cTime == NULL || 
            gdrive_rfc3339_to_epoch_timens
            (cTime, &(pFileinfo->creationTime)) == 0)
    {
        // Didn't get a createdDate or failed to convert it.
        memset(&(pFileinfo->creationTime), 0, sizeof(struct timespec));
    }
    free(cTime);
    
    char* mTime = _gdrive_new_string_from_json(pObj, "modifiedDate", NULL);
    if (mTime == NULL || 
            gdrive_rfc3339_to_epoch_timens
            (mTime, &(pFileinfo->modificationTime)) == 0)
    {
        // Didn't get a modifiedDate or failed to convert it.
        memset(&(pFileinfo->modificationTime), 0, sizeof(struct timespec));
    }
    free(mTime);
    
    char* aTime = _gdrive_new_string_from_json(pObj, 
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

char* _gdrive_new_string_from_json(gdrive_json_object* pObj, 
                                 const char* key,
                                 long* pLength
)
{
    // Find the length of the string.
    long length = gdrive_json_get_string(pObj, key, NULL, 0);
    if (length == INT64_MIN || length == 0)
    {
        // Not a string (length includes the null terminator, so it can't
        // be 0).
        return NULL;
    }

    length *= -1;
    // Allocate enough space to store the retrieved string.
    char* result = malloc(length);
    if (pLength != NULL)
    {
        *pLength = length;
    }
    if (result == NULL)
    {
        // Memory allocation error
        if (pLength != NULL)
        {
            *pLength = 0;
        }
        return NULL;
    }

    // Actually retrieve the string.
    gdrive_json_get_string(pObj, key, result, length);
    return result;
}

Gdrive_Fileinfo* _gdrive_cache_get_item(Gdrive_Info* pInfo, 
                                        const char* fileId,
                                        bool addIfDoesntExist,
                                        bool* pAlreadyExists
)
{
    Gdrive_Cache* pCache = &(pInfo->pInternalInfo->cache);
    // Get the existing node (or a new one) from the cache.
    Gdrive_Cache_Node* pNode = _gdrive_cache_get_node(NULL,
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
    time_t expireTime = (pNode->lastUpdateTime > pCache->lastUpdateTime ?
        pNode->lastUpdateTime : pCache->lastUpdateTime) +
            pInfo->settings.cacheTTL;
    if (expireTime < time(NULL))
    {
        // Update the cache and try again.
        
        // Folder nodes may be deleted by cache updates, but regular file nodes
        // are safe.
        bool isFolder = (pNode->fileinfo.type == GDRIVE_FILETYPE_FOLDER);
        
        _gdrive_update_cache(pInfo);
        
        return (isFolder ? 
                _gdrive_cache_get_item(pInfo, fileId, 
                                       addIfDoesntExist, pAlreadyExists) :
                &(pNode->fileinfo));
    }
    
    // We have a good node that's not too old.
    return &(pNode->fileinfo);
}

Gdrive_Cache_Node* _gdrive_cache_get_node(Gdrive_Cache_Node* pParent,
                                          Gdrive_Cache_Node** ppNode,
                                          const char* fileId,
                                          bool addIfDoesntExist,
                                          bool* pAlreadyExists
)
{
    if (pAlreadyExists != NULL)
    {
        *pAlreadyExists = false;
    }
    
    if (*ppNode == NULL)
    {
        // Item doesn't exist in the cache. Either fail, or create a new item.
        if (!addIfDoesntExist)
        {
            // Not allowed to create a new item, return failure.
            return NULL;
        }
        // else create a new item.
        *ppNode = _gdrive_cache_node_create(pParent);
        if (*ppNode != NULL)
        {
            // Convenience to avoid things like "return &((*ppNode)->fileinfo);"
            Gdrive_Cache_Node* pNode = *ppNode;
            
            // Copy the fileId into the fileinfo. Everything else is left null.
            pNode->fileinfo.id = malloc(strlen(fileId) + 1);
            if (pNode->fileinfo.id == NULL)
            {
                // Memory error.
                _gdrive_cache_node_free(pNode);
                *ppNode = NULL;
                return NULL;
            }
            strcpy(pNode->fileinfo.id, fileId);
            
            // Since this is a new entry, set the node's updated time.
            pNode->lastUpdateTime = time(NULL);
            
            return pNode;
        }
    }
    
    // Convenience to avoid things like "&((*ppNode)->pRight)"
    Gdrive_Cache_Node* pNode = *ppNode;
    
    // Root node exists, try to find the fileId in the tree.
    int cmp = strcmp(fileId, pNode->fileinfo.id);
    if (cmp == 0)
    {
        // Found it at the current node.
        if (pAlreadyExists != NULL)
        {
            *pAlreadyExists = true;
        }
        return pNode;
    }
    else if (cmp < 0)
    {
        // fileId is less than the current node. Look for it on the left.
        return _gdrive_cache_get_node(pNode, &(pNode->pLeft), fileId, 
                                      addIfDoesntExist, pAlreadyExists
                );
    }
    else
    {
        // fileId is greater than the current node. Look for it on the right.
        return _gdrive_cache_get_node(pNode, &(pNode->pRight), fileId, 
                                      addIfDoesntExist, pAlreadyExists
                );
    }
}

void _gdrive_cache_remove_id(Gdrive_Cache_Node** ppHead, const char* fileId)
{
    // Find the node we want to remove.
    Gdrive_Cache_Node* pNode = _gdrive_cache_get_node(NULL, ppHead, fileId, 
                                                      false, NULL
            );
    if (pNode == NULL)
    {
        // Didn't find it.  Do nothing.
        return;
    }
    
    // Find whether the node is at the root of the tree.  If it's at the root,
    // the pointer from its "parent" is *ppHead.  Otherwise, find which side
    // it's on and get the address of the correct pointer.
    Gdrive_Cache_Node** ppFromParent = ppHead;
    if (pNode->pParent != NULL)
    {
        if (pNode->pParent->pLeft == pNode)
        {
            ppFromParent = &(pNode->pParent->pLeft);
        }
        else
        {
            ppFromParent = &(pNode->pParent->pRight);
        }
    }
    _gdrive_cache_delete_node(ppFromParent, pNode);
}

void _gdrive_cache_delete_node(Gdrive_Cache_Node* * ppFromParent, 
                               Gdrive_Cache_Node* pNode
)
{
    // Simplest special case. pNode has no descendents.  Just delete it, and
    // set the pointer from the parent to NULL.
    if (pNode->pLeft == NULL && pNode->pRight == NULL)
    {
        *ppFromParent = NULL;
        _gdrive_cache_node_free(pNode);
        return;
    }
    
    // Second special case. pNode has one side empty. Promote the descendent on
    // the other side into pNode's place.
    if (pNode->pLeft == NULL)
    {
        *ppFromParent = pNode->pRight;
        pNode->pRight->pParent = pNode->pParent;
        _gdrive_cache_node_free(pNode);
        return;
    }
    if (pNode->pRight == NULL)
    {
        *ppFromParent = pNode->pLeft;
        pNode->pLeft->pParent = pNode->pParent;
        _gdrive_cache_node_free(pNode);
        return;
    }
    
    // General case with descendents on both sides. Find the node with the 
    // closest value to pNode in one of its subtrees (leftmost node of the right
    // subtree, or rightmost node of the left subtree), and switch places with
    // pNode.  Which side we use doesn't really matter.  We'll rather 
    // arbitrarily decide to use the same side subtree as the side from which
    // pNode hangs off its parent (if pNode is on the right side of its parent,
    // find the leftmost node of the right subtree), and treat the case where
    // pNode is the root the same as if it were on the left side of its parent.
    Gdrive_Cache_Node* pSwap = NULL;
    Gdrive_Cache_Node** ppToSwap = NULL;
    if (pNode->pParent != NULL && pNode->pParent->pRight == pNode)
    {
        // Find the leftmost node of the right subtree.
        pSwap = pNode->pRight;
        ppToSwap = &(pNode->pRight);
        while (pSwap->pLeft != NULL)
        {
            ppToSwap = &(pSwap->pLeft);
            pSwap = pSwap->pLeft;
        }
    }
    else
    {
        // Find the rightmost node of the left subtree.
        pSwap = pNode->pLeft;
        ppToSwap = &(pNode->pLeft);
        while (pSwap->pRight != NULL)
        {
            ppToSwap = &(pSwap->pRight);
            pSwap = pSwap->pRight;
        }
    }
    
    // Swap the nodes
    _gdrive_cache_node_swap(ppFromParent, pNode, ppToSwap, pSwap);
    
    // Find the pointer from pNode's new parent.  We don't need to worry about
    // a NULL pParent, since pNode can't be at the root of the tree after
    // swapping.
    Gdrive_Cache_Node** ppFromNewParent = (pNode->pParent->pLeft == pNode ?
        &(pNode->pParent->pLeft) :
        &(pNode->pParent->pRight)
            );
    
    _gdrive_cache_delete_node(ppFromNewParent, pNode);
}

void _gdrive_cache_node_swap(Gdrive_Cache_Node** ppFromParentOne,
                             Gdrive_Cache_Node* pNodeOne,
                             Gdrive_Cache_Node** ppFromParentTwo,
                             Gdrive_Cache_Node* pNodeTwo
)
{
    // Swap the pointers from the parents
    *ppFromParentOne = pNodeTwo;
    *ppFromParentTwo = pNodeOne;
    
    Gdrive_Cache_Node* pTempParent = pNodeOne->pParent;
    Gdrive_Cache_Node* pTempLeft = pNodeOne->pLeft;
    Gdrive_Cache_Node* pTempRight = pNodeOne->pRight;
    
    pNodeOne->pParent = pNodeTwo->pParent;
    pNodeOne->pLeft = pNodeTwo->pLeft;
    pNodeOne->pRight = pNodeTwo->pRight;
    
    pNodeTwo->pParent = pTempParent;
    pNodeTwo->pLeft = pTempLeft;
    pNodeTwo->pRight = pTempRight;
}

/*
 * Set pParent to NULL for the root node of the tree (the node that has no
 * parent).
 */
Gdrive_Cache_Node* _gdrive_cache_node_create(Gdrive_Cache_Node* pParent)
{
    Gdrive_Cache_Node* result = malloc(sizeof(Gdrive_Cache_Node));
    if (result != NULL)
    {
        memset(result, 0, sizeof(Gdrive_Cache_Node));
        result->pParent = pParent;
    }
    return result;
}

/*
 * NOT RECURSIVE.  FREES ONLY THE SINGLE NODE.
 */
void _gdrive_cache_node_free(Gdrive_Cache_Node* pNode)
{
    gdrive_fileinfo_cleanup(&(pNode->fileinfo));
    pNode->pLeft = NULL;
    pNode->pRight = NULL;
    free(pNode);
}

const char* _gdrive_fileid_cache_get_item(Gdrive_Info* pInfo, 
                                          const char* path
)
{
    Gdrive_Cache* pCache = &(pInfo->pInternalInfo->cache);
    
    // Get the cached node if it exists.  If it doesn't exist, fail.
    Gdrive_Fileid_Cache_Node headNode = pCache->fileIdCacheHead;
    Gdrive_Fileid_Cache_Node* pNode = _gdrive_fileid_cache_get_node(&headNode,
                                                                    path
            );
    if (pNode == NULL)
    {
        // The path isn't cached.  Return null.
        return NULL;
    }
    
    // We have the cached item.  Test whether it's too old.  Use the last update
    // either of the entire cache, or of the individual item, whichever is
    // newer.
    time_t expireTime = ((pNode->lastUpdateTime > pCache->lastUpdateTime) ?
        pNode->lastUpdateTime : pCache->lastUpdateTime) + 
            pInfo->settings.cacheTTL;
    if (time(NULL) > expireTime)
    {
        // Item is expired.  Check for updates and try again.
        _gdrive_update_cache(pInfo);
        return _gdrive_fileid_cache_get_item(pInfo, path);
    }
    
    // Item exists and is not expired.
    return pNode->fileId;
}
        
Gdrive_Fileid_Cache_Node* _gdrive_fileid_cache_get_node(
        Gdrive_Fileid_Cache_Node* pHead, 
        const char* path
)
{
    Gdrive_Fileid_Cache_Node* pNode = pHead->pNext;
    
    while (pNode != NULL)
    {
        int cmp = strcmp(path, pNode->path);
        if (cmp == 0)
        {
            // Found it!
            return pNode;
        }
        else if (cmp < 0)
        {
            // We've gone too far.  It's not here.
            return NULL;
        }
        // else keep searching
        pNode = pNode->pNext;
    }
    
    // We've hit the end of the list without finding path.
    return NULL;
}

int _gdrive_fileid_cache_add_item(Gdrive_Fileid_Cache_Node* pHead,
                                  const char* path,
                                  const char* fileId
)
{
    Gdrive_Fileid_Cache_Node* pPrev = pHead;
    
    while (true)
    {
        Gdrive_Fileid_Cache_Node* pNext = pPrev->pNext;
        // Find the string comparison.  If pNext is NULL, pretend pNext->path
        // is greater than path (we insert after pPrev in both cases).
        int cmp = (pNext != NULL) ? strcmp(path, pNext->path) : -1;
        
        if (cmp == 0)
        {
            // Item already exists, update it.
            return _gdrive_fileid_cache_update_item(pNext, fileId);
        }
        else if (cmp < 0)
        {
            // Item doesn't exist yet, insert it between pPrev and pNext.
            Gdrive_Fileid_Cache_Node* pNew = 
                    _gdrive_fileid_cache_node_create(path, fileId);
            if (pNew == NULL)
            {
                // Error, most likely memory.
                return -1;
            }
            pPrev->pNext = pNew;
            pNew->pNext = pNext;
            return 0;
        }
        // else keep searching
        pPrev = pNext;
    }
}

Gdrive_Fileid_Cache_Node* _gdrive_fileid_cache_node_create(const char* filename,
                                                           const char* fileId)
{
    Gdrive_Fileid_Cache_Node* pResult = malloc(sizeof(Gdrive_Fileid_Cache_Node));
    if (pResult != NULL)
    {
        memset(pResult, 0, sizeof(Gdrive_Fileid_Cache_Node));
        pResult->path = malloc(strlen(filename) + 1);
        if (pResult->path == NULL)
        {
            // Memory error.
            free(pResult);
            return NULL;
        }
        strcpy(pResult->path, filename);
        
        // Only try copying the fileId if it was specified.
        if (fileId != NULL)
        {
            pResult->fileId = malloc(strlen(fileId) + 1);
            if (pResult->fileId == NULL)
            {
                // Memory error.
                free(pResult->path);
                free(pResult);
                return NULL;
            }
            strcpy(pResult->fileId, fileId);
        }
        
        // Set the updated time.
        pResult->lastUpdateTime = time(NULL);
    }
    return pResult;
}

int _gdrive_fileid_cache_update_item(Gdrive_Fileid_Cache_Node* pNode, 
                                     const char* fileId
)
{
    // Update the time.
    pNode->lastUpdateTime = time(NULL);
    
    if ((pNode->fileId == NULL) || (strcmp(fileId, pNode->fileId) != 0))
    {
        // pNode doesn't have a fileId or the IDs don't match. Copy the new
        // fileId in.
        free(pNode->fileId);
        pNode->fileId = malloc(strlen(fileId) + 1);
        if (pNode->fileId == NULL)
        {
            // Memory error.
            return -1;
        }
        strcpy(pNode->fileId, fileId);
        return 0;
    }
    // else the IDs already match.
    return 0;
}

void _gdrive_fileid_cache_remove_id(Gdrive_Fileid_Cache_Node* pHead, 
                                    const char* fileId
)
{
    // Need to walk through the whole list, since it's not keyed by fileId.
    Gdrive_Fileid_Cache_Node* pPrev = pHead;
    Gdrive_Fileid_Cache_Node* pNext = pPrev->pNext;
    
    while (pNext != NULL)
    {
        
        // Compare the given fileId to the current one.
        int cmp = strcmp(fileId, pNext->fileId);
        
        if (cmp == 0)
        {
            // Found it!
            pPrev->pNext = pNext->pNext;
            _gdrive_fileid_cache_node_free(pNext);
            
            // No need to keep going.
            return;
        }
        // else keep searching
        pPrev = pNext;
        pNext = pPrev->pNext;
    }
}

/*
 * DOES NOT REMOVE FROM LIST.  FREES ONLY THE SINGLE NODE.
 */
void _gdrive_fileid_cache_node_free(Gdrive_Fileid_Cache_Node* pNode)
{
    free(pNode->fileId);
    pNode->fileId = NULL;
    free(pNode->path);
    pNode->path = NULL;
    pNode->pNext = NULL;
    free(pNode);
}

int _gdrive_cache_init(Gdrive_Info* pInfo)
{
    CURL* curlHandle = pInfo->pInternalInfo->curlHandle;
    
    Gdrive_Query* pQuery = _gdrive_query_create(curlHandle);
    _gdrive_query_add(pQuery, "includeSubscribed", "false");
    _gdrive_query_add(pQuery, "fields", "largestChangeId");
    
    // Do the transfer.
    Gdrive_Download_Buffer* pBuf = 
            _gdrive_do_transfer(pInfo, GDRIVE_REQUEST_GET, true, 
                                GDRIVE_URL_ABOUT, pQuery, NULL, NULL
            );
    _gdrive_query_free(pQuery);
    
    int returnVal = -1;
    if (pBuf != NULL && pBuf->httpResp < 400)
    {
        // Response was good, try extracting the data.
        gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
        if (pObj != NULL)
        {
            bool success = false;
            pInfo->pInternalInfo->cache.nextChangeId = 
                    gdrive_json_get_int64(pObj, "largestChangeId", 
                                          true, &success
                    ) + 1;
            returnVal = success ? 0 : -1;
            gdrive_json_kill(pObj);
        }
    }
    
    _gdrive_download_buffer_free(pBuf);
    return returnVal;
}

int _gdrive_update_cache(Gdrive_Info* pInfo)
{
    // Convenience assignments
    CURL* curlHandle = pInfo->pInternalInfo->curlHandle;
    Gdrive_Cache* pCache = &(pInfo->pInternalInfo->cache);
    
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
    
    Gdrive_Query* pQuery = _gdrive_query_create(curlHandle);
    if (pQuery == NULL)
    {
        // Memory error
        free(changeIdString);
        return -1;
    }
    _gdrive_query_add(pQuery, "startChangeId", changeIdString);
    _gdrive_query_add(pQuery, "includeSubscribed", "false");
    _gdrive_query_add(pQuery, 
                      "fields", 
                      "largestChangeId,items(fileId,deleted,file)"
            );
    free(changeIdString);
    
    Gdrive_Download_Buffer* pBuf = 
            _gdrive_do_transfer(pInfo, GDRIVE_REQUEST_GET, true, 
                                GDRIVE_URL_CHANGES, pQuery, NULL, NULL
            );
    _gdrive_query_free(pQuery);
    
    int returnVal = -1;
    if (pBuf != NULL && pBuf->httpResp < 400)
    {
        // Response was good, try extracting the data.
        gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
        if (pObj != NULL)
        {
            // Update or remove cached data for each item in the "items" array.
            gdrive_json_object* pChangeArray = 
                    gdrive_json_get_nested_object(pObj, "items");
            int arraySize = gdrive_json_array_length(pChangeArray, NULL);
            for (int i = 0; i < arraySize; i++)
            {
                gdrive_json_object* pItem = 
                        gdrive_json_array_get(pChangeArray, NULL, i);
                if (pItem == NULL)
                {
                    // Couldn't get this item, skip to the next one.
                    continue;
                }
                char* fileId = 
                        _gdrive_new_string_from_json(pItem, "fileId", NULL);
                if (fileId == NULL)
                {
                    // Couldn't get an ID for the changed file, skip to the
                    // next one.
                    continue;
                }
                
                // We don't know whether the file has been renamed or moved,
                // so remove it from the fileId cache.
                _gdrive_fileid_cache_remove_id(&(pCache->fileIdCacheHead), 
                                               fileId
                        );
                
                // Update the file metadata cache.
                Gdrive_Cache_Node* pCacheNode = 
                        _gdrive_cache_get_node(NULL,
                                               &(pCache->pCacheHead), 
                                               fileId, 
                                               false, 
                                               NULL
                        );
                
                // If this file was in the cache, update its information
                if (pCacheNode != NULL)
                {
                    gdrive_fileinfo_cleanup(&(pCacheNode->fileinfo));
                    _gdrive_get_fileinfo_from_json(
                            pInfo,
                            gdrive_json_get_nested_object(pItem, "file"),
                            &(pCacheNode->fileinfo)
                            );
                }
                
                // The file's parents may now have a different number of 
                // children.  Remove the parents from the cache.
                int numParents = gdrive_json_array_length(pItem, "parents");
                for (int nParent = 0; nParent < numParents; nParent++)
                {
                    // Get the fileId of the current parent in the array.
                    char* parentId = NULL;
                    gdrive_json_object* pParentObj = 
                            gdrive_json_array_get(pItem, "parents", nParent);
                    if (pParentObj != NULL)
                    {
                        parentId = _gdrive_new_string_from_json(pParentObj, 
                                                                "id", 
                                                                NULL);
                    }
                    // Remove the parent from the cache, if present.
                    if (parentId != NULL)
                    {
                        _gdrive_cache_remove_id(&(pCache->pCacheHead), 
                                                parentId
                                );
                    }
                    free(parentId);
                }
                
                free(fileId);
            }
            
            bool success = false;
            pCache->nextChangeId = gdrive_json_get_int64(pObj, 
                                                         "largestChangeId", 
                                                         true, &success
                    ) + 1;
            returnVal = success ? 0 : -1;
            gdrive_json_kill(pObj);
        }
    }
    
    // Reset the last updated time
    pCache->lastUpdateTime = time(NULL);
    
    _gdrive_download_buffer_free(pBuf);
    return returnVal;
}

long _gdrive_divide_round_up(long dividend, long divisor)
{
    // Could use ceill() or a similar function for this, but I don't  know 
    // whether there might be some values that don't convert exactly between
    // long int and long double and back.
    
    // Integer division rounds down.  If there's a remainder, add 1.
    return (dividend % divisor == 0) ? 
        (dividend / divisor) : 
        (dividend / divisor + 1);
}