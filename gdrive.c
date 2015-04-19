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

//#include "fuse-drive.h"
#include "gdrive.h"
#include "gdrive-internal.h"
#include "gdrive-json.h"

/*
 * gdrive_init():   Initializes the network connection, sets appropriate 
 *                  settings for the Google Drive session, and ensures the user 
 *                  has granted necessary access permissions for the Google 
 *                  Drive account.
 */
int gdrive_init(Gdrive_Info** ppGdriveInfo, 
                int access, 
                const char* authFilename, 
                enum Gdrive_Interaction interactionMode
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
                              interactionMode
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
                enum Gdrive_Interaction interactionMode
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
    _gdrive_read_auth_file(authFilename, pInfo->pInternalInfo);
        
    // Authenticate or refresh access
    pInfo->settings.mode = access;
    if (gdrive_auth(pInfo) != 0)
    {
        // Could not get the required permissions.  Return error.
        return -1;
    }
    // Can we continue prompting for authentication if needed later?
    pInfo->settings.userInteractionAllowed = 
            (interactionMode == GDRIVE_INTERACTION_ALWAYS);
    
    
    return 0;
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
        Gdrive_Download_Buffer* downloadBuffer = 
                _gdrive_download_buffer_create(200);
        if (downloadBuffer == NULL)
        {
            // Memory error
            return -1;
        }
        int refreshSuccess = _gdrive_refresh_auth_token(pInfo->pInternalInfo, 
                                                        downloadBuffer,
                                                        GDRIVE_GRANTTYPE_REFRESH,
                                                        pInfo->pInternalInfo->refreshToken,
                                                        0, GDRIVE_RETRY_LIMIT
        );
        _gdrive_download_buffer_free(downloadBuffer);
        
        if (refreshSuccess == 0)
        {
            // Refresh succeeded, but we don't know what scopes were previously
            // granted.  Check to make sure we have the required scopes.  If so,
            // then we don't need to do anything else and can return success.
            Gdrive_Download_Buffer* pBuf = _gdrive_download_buffer_create(200);
            if (pBuf == NULL)
            {
                // Memory error
                return -1;
            }
            int success = _gdrive_check_scopes(pBuf, pInfo, 
                                               0, GDRIVE_RETRY_LIMIT
            );
            _gdrive_download_buffer_free(pBuf);
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


void gdrive_cleanup(Gdrive_Info* pInfo)
{
    gdrive_cleanup_nocurl(pInfo);
    curl_global_cleanup();
}

void gdrive_cleanup_nocurl(Gdrive_Info* pInfo)
{
    _gdrive_info_free(pInfo);
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
    
    
    // Make sure the file exists and is a regular file (or if it's a 
    // symlink, it points to a regular file).
    struct stat st;
    if ((lstat(filename, &st) == 0) && (st.st_mode & S_IFREG))
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
            int tokenLength = -gdrive_json_get_string(pObj, 
                                                      GDRIVE_FIELDNAME_ACCESSTOKEN, 
                                                      NULL, 0
                    );
            if (tokenLength > 0)
            {
                pInfo->accessToken = malloc(tokenLength);
                pInfo->accessTokenLength = tokenLength;
                gdrive_json_get_string(pObj, GDRIVE_FIELDNAME_ACCESSTOKEN, 
                                       pInfo->accessToken, tokenLength
                        );
            }
            tokenLength = -gdrive_json_get_string(pObj, 
                                                  GDRIVE_FIELDNAME_REFRESHTOKEN,
                                                  NULL, 0
                    );
            if (tokenLength > 0)
            {
                pInfo->refreshToken = malloc(tokenLength);
                pInfo->refreshTokenLength = tokenLength;
                gdrive_json_get_string(pObj, GDRIVE_FIELDNAME_REFRESHTOKEN, 
                                       pInfo->refreshToken, tokenLength
                        );
            }
            
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
    
    // For now, do nothing.  We'll add members to this struct later, and likely
    // some of them will need cleaned up.
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
                                    long* pHttpResp
)
{
    // Make sure data gets written at the start of the buffer.
    pBuffer->usedSize = 0;
    
    // Do the download.
    curl_easy_setopt(curlHandle, 
                     CURLOPT_WRITEFUNCTION, 
                     _gdrive_download_buffer_callback
            );
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, pBuffer);
    CURLcode returnVal = curl_easy_perform(curlHandle);
    
    // Get the HTTP response
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, pHttpResp);
    
    return returnVal;
}

size_t _gdrive_download_buffer_callback(char *newData, 
                                        size_t size, 
                                        size_t nmemb, 
                                        void *userdata
)
{
    if (size == 0 || nmemb == 0)
    {
        // No data
        return 0;
    }
    
    Gdrive_Download_Buffer* pBuffer = (Gdrive_Download_Buffer*) userdata;
    
    // Find the length of the data, and allocate more memory if needed.
    size_t dataSize = size * nmemb;
    size_t totalSize = dataSize + pBuffer->usedSize;
    if (totalSize > pBuffer->allocatedSize)
    {
        // Allow extra room to reduce the number of realloc's.
        size_t allocSize = totalSize + dataSize;    
        pBuffer->data = realloc(pBuffer->data, totalSize);
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
    pBuf->data = NULL;
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
        strcpy(result, encodedStrings[2 * i]);
        strcpy(result, "=");
        
        // Add the value and (if applicable) "&".
        strcpy(result, encodedStrings[2 * i + 1]);
        if (i < n - 1)
        {
            strcpy(result, "&");
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
        strcpy(result, encodedStrings[2 * i]);
        strcpy(result, "=");
        
        // Add the value and (if applicable) "&".
        strcpy(result, encodedStrings[2 * i + 1]);
        if (i < n - 1)
        {
            strcpy(result, "&");
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

int _gdrive_refresh_auth_token(Gdrive_Info_Internal* pInternalInfo, 
                               Gdrive_Download_Buffer* pBuf,
                               const char* grantType,
                               const char* tokenString,
                               int tryNum,
                               int maxTries
)
{
    // Make sure we were given a valid grant_type
    if (strcmp(grantType, GDRIVE_GRANTTYPE_CODE) && 
            strcmp(grantType, GDRIVE_GRANTTYPE_REFRESH))
    {
        // Invalid grant_type
        return -1;
    }
    
    if (pInternalInfo->curlHandle == NULL)
    {
        if ((pInternalInfo->curlHandle = curl_easy_init()) == NULL)
        {
            // Couldn't get a curl easy handle, return error.
            return -1;
        }
    }
    
    // Set up the POST data.  It feels like there should be a cleaner (more
    // uniform) way to do this.
    char* postData;
    postData = (strcmp(grantType, GDRIVE_GRANTTYPE_CODE) == 0) ?
        _gdrive_postdata_assemble(
            pInternalInfo->curlHandle, 5, 
            GDRIVE_FIELDNAME_CODE, tokenString,
            "redirect_uri", GDRIVE_REDIRECT_URI,
            GDRIVE_FIELDNAME_CLIENTID, GDRIVE_CLIENT_ID,
            GDRIVE_FIELDNAME_CLIENTSECRET, GDRIVE_CLIENT_SECRET,
            GDRIVE_FIELDNAME_GRANTTYPE, grantType
            ) : 
        _gdrive_postdata_assemble(
            pInternalInfo->curlHandle, 4, 
            GDRIVE_FIELDNAME_REFRESHTOKEN, tokenString,
            GDRIVE_FIELDNAME_CLIENTID, GDRIVE_CLIENT_ID,
            GDRIVE_FIELDNAME_CLIENTSECRET, GDRIVE_CLIENT_SECRET,
            GDRIVE_FIELDNAME_GRANTTYPE, grantType
            );
    if (postData == NULL)
    {
        // Memory error
        return -1;
    }
    
    CURL* curlHandle = pInternalInfo->curlHandle;
    
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curlHandle, CURLOPT_URL, GDRIVE_URL_AUTH_TOKEN);
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, postData);

    long httpResult = 0;
    CURLcode curlResult = _gdrive_download_to_buffer(curlHandle, 
                                                     pBuf, 
                                                     &httpResult
            );
    curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, NULL);
    free(postData);
    
    // Prepare to return a non-error failure.  If an error occurs or the
    // refresh succeeds, we'll change this.
    int returnVal = 1;
    
    if (curlResult != CURLE_OK)
    {
        // There was an error sending the request and getting the response.
        returnVal = -1;
    }
    else if (httpResult >= 400)
    {
        // Some HTTP response codes should be retried, and some should not.
        // Any 5xx codes should be retried, and *sometimes* 403 should be.
        bool retry = (httpResult >= 500);
        if (httpResult == 403)
        {
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
        }
        
        if (retry)
        {
            if (tryNum == maxTries)
            {
                // This is a request that should be retried, but the maximum
                // number of attempts has already been made.  Could be a server
                // error or similar.  Return error.
                returnVal = -1;
            }
            else
            {
                _gdrive_exponential_wait(tryNum);
                // Retry, and return whatever the next try returns.
                returnVal = _gdrive_refresh_auth_token(pInternalInfo, pBuf,
                                                       grantType, tokenString, 
                                                       tryNum + 1, maxTries);
            }
        }
        else
        {
            // Failure, but probably not an error.  Most likely, the user has
            // revoked permission or the refresh token has otherwise been
            // invalidated.
            returnVal = 1;
        }
    }
    else
    {
        // If we've gotten this far, we have a good HTTP response.  Now we just
        // need to pull the access_token string (and refresh token string if
        // present) out of it.
        
        gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
        if (pObj == NULL)
        {
            // Couldn't locate JSON-formatted information in the server's 
            // response.  Return error.
            return -1;
        }
        long length = gdrive_json_get_string(pObj, 
                                              GDRIVE_FIELDNAME_ACCESSTOKEN, 
                                              NULL, 0
        );
        if (length == INT64_MIN || length == 0)
        {
            // We shouldn't get an empty string or non-string.  Treat this
            // as an error.
            returnVal = -1;
        }
        else
        {
            length *= -1;
            // Make sure we have enough space to store the access_token string.
            if (pInternalInfo->accessTokenLength < length)
            {
                pInternalInfo->accessToken = realloc(pInternalInfo->accessToken,
                                                     length
                        );
                pInternalInfo->accessTokenLength = length;
                if (pInternalInfo->accessToken == NULL)
                {
                    // Memory allocation error
                    pInternalInfo->accessTokenLength = 0;
                    gdrive_json_kill(pObj);
                    return -1;
                }
            }
            gdrive_json_get_string(pObj, 
                                   GDRIVE_FIELDNAME_ACCESSTOKEN,
                                   pInternalInfo->accessToken,
                                   pInternalInfo->accessTokenLength
                    );
            // Prepare to return success
            returnVal = 0;
            
            // We won't always have a refresh token.  Specifically, if we were
            // already sending a refresh token, we may not get one back.
            // Don't treat the lack of a refresh token as an error or a failure,
            // and don't clobber the existing refresh token if we don't get a
            // new one.
            length = gdrive_json_get_string(pObj, 
                                            GDRIVE_FIELDNAME_REFRESHTOKEN, 
                                            NULL, 0
                    );
            if (length < 0 && length != INT64_MIN)
            {
                // We were given a refresh token
                length *= -1;
                // Make sure we have enough space to store the access_token string.
                if (pInternalInfo->refreshTokenLength < length)
                {
                    pInternalInfo->refreshToken = 
                            realloc(pInternalInfo->refreshToken, length);
                    pInternalInfo->refreshTokenLength = length;
                    if (pInternalInfo->refreshToken == NULL)
                    {
                        // Memory allocation error
                        pInternalInfo->refreshTokenLength = 0;
                        gdrive_json_kill(pObj);
                        return -1;
                    }
                }
                gdrive_json_get_string(pObj, 
                                       GDRIVE_FIELDNAME_REFRESHTOKEN,
                                       pInternalInfo->refreshToken,
                                       pInternalInfo->refreshTokenLength
                        );
            }
        }
        gdrive_json_kill(pObj);
    }
    return returnVal;
}

int _gdrive_prompt_for_auth(Gdrive_Info* pInfo)
{
    // List the possible access mode flags and related scopes (both arrays must
    // keep the same order).
    /*const int accessModes[] = {GDRIVE_ACCESS_META,
                               GDRIVE_ACCESS_READ,
                               GDRIVE_ACCESS_WRITE,
                               GDRIVE_ACCESS_APPS
                               };*/
    /*const char* const scopes[] = {GDRIVE_SCOPE_META, 
                                  GDRIVE_SCOPE_READ, 
                                  GDRIVE_SCOPE_WRITE,
                                  GDRIVE_SCOPE_APPS 
                                  };*/
    // Number of elements in each of the accessModes and scopes arrays.
    //const int numModes = 4;
    
    
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
    Gdrive_Download_Buffer* pBuf = _gdrive_download_buffer_create(200);
    int returnVal = _gdrive_refresh_auth_token(pInfo->pInternalInfo, 
                                               pBuf, GDRIVE_GRANTTYPE_CODE, 
                                               authCode, 0, GDRIVE_RETRY_LIMIT);
    _gdrive_download_buffer_free(pBuf);
    
    return returnVal;
}

int _gdrive_check_scopes(Gdrive_Download_Buffer* pBuf,
                         Gdrive_Info* pInfo, 
                         int tryNum, 
                         int maxTries
)
{
    Gdrive_Info_Internal* pInternalInfo = pInfo->pInternalInfo;
    CURL* curlHandle = pInternalInfo->curlHandle;
    curl_easy_setopt(curlHandle, CURLOPT_HTTPGET, 1);
    char* url = _gdrive_assemble_query_string(curlHandle, 
                                              GDRIVE_URL_AUTH_TOKENINFO, 
                                              1, 
                                              GDRIVE_FIELDNAME_ACCESSTOKEN,
                                              pInternalInfo->accessToken
    );
    if (url == NULL)
    {
        // Memory error
        return -1;
    }
    
    curl_easy_setopt(curlHandle, CURLOPT_URL, url);
    
    long httpResp = 0;
    CURLcode curlCode = _gdrive_download_to_buffer(curlHandle, pBuf, &httpResp);
    if (curlCode != CURLE_OK)
    {
        // Download error
        _gdrive_download_buffer_free(pBuf);
        return -1;
    }
    else if (httpResp >= 500 && tryNum < maxTries)
    {
        // Retry on 5xx server errors.  Use exponential backoff to determine
        // how long to wait before retrying.
        _gdrive_exponential_wait(tryNum);
        return _gdrive_check_scopes(pBuf, pInfo, tryNum + 1, maxTries);
    }
    else if (httpResp >= 400)
    {
        // Either a 4xx error that shouldn't be retried, or a 5xx error after
        // the maximum number of attempts has already been made.  Return error.
        return -1;
    }
    
    // If we've made it this far, we have an ok response.  Extract the scopes
    // from the JSON array that should have been returned, and compare them
    // with the expected scopes.
    
    gdrive_json_object* pObj = gdrive_json_from_string(pBuf->data);
    if (pObj == NULL)
    {
        // Couldn't interpret the response as JSON, return error.
        return -1;
    }
    long length =  gdrive_json_get_string(pObj, "scope", NULL, 0);
    if (length == INT64_MIN || length == 0)
    {
        // Key not found, or value not a string.  Return error.
        gdrive_json_kill(pObj);
        return -1;
    }
    length *= -1;
    char* grantedScopes = malloc(length);
    gdrive_json_get_string(pObj, "scope", grantedScopes, length);
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