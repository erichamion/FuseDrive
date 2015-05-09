/* 
 * File:   gdrive-internal.h
 * Author: me
 * 
 * Declarations and definitions to be used by various gdrive code and header
 * files, but which are not part of the public gdrive interface.
 *
 * Created on April 14, 2015, 9:31 PM
 */

#ifndef GDRIVE_INTERNAL_H
#define	GDRIVE_INTERNAL_H

#ifdef	__cplusplus
extern "C" {
#endif

    
/*
 * The client-secret.h header file is NOT included in the Git
 * repository. It should define the following as preprocessor macros
 * for string constants:
 * GDRIVE_CLIENT_ID 
 * GDRIVE_CLIENT_SECRET
 */
    
#include <curl/curl.h>
    
#include "gdrive.h"
#include "gdrive-client-secret.h"
#include "gdrive-util.h"
#include "gdrive-download-buffer.h"
#include "gdrive-query.h"
#include "gdrive-cache.h"
#include "gdrive-cache-node.h"
    

    
#define GDRIVE_URL_FILES "https://www.googleapis.com/drive/v2/files"
#define GDRIVE_URL_ABOUT "https://www.googleapis.com/drive/v2/about"
#define GDRIVE_URL_CHANGES "https://www.googleapis.com/drive/v2/changes"
    




typedef struct Gdrive_Info_Internal Gdrive_Info_Internal;
typedef struct Gdrive_Info_Internal
{
    char* accessToken;
    char* refreshToken;
    long accessTokenLength;  // Number of bytes allocated for accessToken
    long refreshTokenLength; // Number of bytes allocated for refreshToken
    const char* clientId;
    const char* clientSecret;
    const char* redirectUri;
    bool isCurlInitialized;
    CURL* curlHandle;
    //Gdrive_Sysinfo* pSysinfo;
    Gdrive_Cache* pCache;
} Gdrive_Info_Internal;




Gdrive_Download_Buffer* _gdrive_do_transfer(
        Gdrive_Info* pInfo, enum Gdrive_Request_Type requestType,
        bool retryOnAuthError, const char* url,  const Gdrive_Query* pQuery, 
        struct curl_slist* pHeaders, FILE* destFile
);




/*
 * gdrive_auth():   Authenticate and obtain permissions from the user for Google
 *                  Drive.  If passed the address of a Gdrive_Info struct whose
 *                  pInternalInfo member has existing authentication 
 *                  information, will attempt to reuse this information first.
 * Parameters:
 *      pInfo (Gdrive_Info*):
 *              Address of the Gdrive_Info struct previously created by 
 *              gdrive_init().  The following members are used:
 *              settings.mode:  
 *                      Describes the permissions that need to be requested (see
 *                      the access parameter for gdrive_init()).
 *              settings.userInteractionAllowed:    
 *                      If false, the function will return error when the 
 *                      necessary permissions have not already granted.  If
 *                      true, will prompt the user for permission if needed.
 *              pInternalInfo:
 *                      Existing authentication information will be read from 
 *                      this member, and new authentication information will be
 *                      written back to it.
 * Returns:
 *      0 for success, other value on error.
 */
int gdrive_auth(Gdrive_Info* pInfo);








#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_INTERNAL_H */

