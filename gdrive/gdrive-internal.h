/* 
 * File:   gdrive-internal.h
 * Author: me
 * 
 * Declarations and definitions to be used internally within GDrive.c
 * 
 * This file must be included AFTER <curl/curl.h> and "GDrive.h"
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
#include "gdrive-client-secret.h"
#include "gdrive-download-buffer.h"
#include "gdrive-query.h"
#include "gdrive-cache.h"
#include "gdrive-cache-node.h"
    
#define GDRIVE_REDIRECT_URI "urn:ietf:wg:oauth:2.0:oob"
#define GDRIVE_MAX_TOKEN_LENGTH 1024

#define GDRIVE_FIELDNAME_ACCESSTOKEN "access_token"
#define GDRIVE_FIELDNAME_REFRESHTOKEN "refresh_token"
#define GDRIVE_FIELDNAME_CODE "code"
#define GDRIVE_FIELDNAME_CLIENTID "client_id"
#define GDRIVE_FIELDNAME_CLIENTSECRET "client_secret"
#define GDRIVE_FIELDNAME_GRANTTYPE "grant_type"
#define GDRIVE_FIELDNAME_REDIRECTURI "redirect_uri"
    
#define GDRIVE_GRANTTYPE_CODE "authorization_code"
#define GDRIVE_GRANTTYPE_REFRESH "refresh_token"
    
#define GDRIVE_URL_AUTH_TOKEN "https://www.googleapis.com/oauth2/v3/token"
#define GDRIVE_URL_AUTH_TOKENINFO "https://www.googleapis.com/oauth2/v1/tokeninfo"
#define GDRIVE_URL_AUTH_NEWAUTH "https://accounts.google.com/o/oauth2/auth"
#define GDRIVE_URL_FILES "https://www.googleapis.com/drive/v2/files"
#define GDRIVE_URL_ABOUT "https://www.googleapis.com/drive/v2/about"
#define GDRIVE_URL_CHANGES "https://www.googleapis.com/drive/v2/changes"
    
#define GDRIVE_SCOPE_META "https://www.googleapis.com/auth/drive.readonly.metadata"
#define GDRIVE_SCOPE_READ "https://www.googleapis.com/auth/drive.readonly"
#define GDRIVE_SCOPE_WRITE "https://www.googleapis.com/auth/drive"
#define GDRIVE_SCOPE_APPS "https://www.googleapis.com/auth/drive.apps.readonly"
#define GDRIVE_SCOPE_MAXLENGTH 200
    
#define GDRIVE_MIMETYPE_FOLDER "application/vnd.google-apps.folder"


    
#define GDRIVE_403_RATELIMIT "rateLimitExceeded"
#define GDRIVE_403_USERRATELIMIT "userRateLimitExceeded"
    
#define GDRIVE_RETRY_LIMIT 5
    
enum Gdrive_Retry_Method
{
    GDRIVE_RETRY_NORETRY,
    GDRIVE_RETRY_RETRY,
    GDRIVE_RETRY_RENEWAUTH
};

enum Gdrive_Request_Type
{
    GDRIVE_REQUEST_GET,
    GDRIVE_REQUEST_POST,
    GDRIVE_REQUEST_PATCH
};











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







int _gdrive_info_create(Gdrive_Info** ppInfo);
int _gdrive_info_internal_create(Gdrive_Info_Internal** ppInfo);
int _gdrive_read_auth_file(const char* filename, Gdrive_Info_Internal* pInfo);
void _gdrive_info_free(Gdrive_Info* pInfo);
void _gdrive_settings_cleanup(Gdrive_Settings* pSettings);
void _gdrive_info_internal_free(Gdrive_Info* pInfo);


int _gdrive_refresh_auth_token(Gdrive_Info* pInfo, 
                               const char* grantType,
                               const char* tokenString
);
int _gdrive_prompt_for_auth(Gdrive_Info* pInfo);
int _gdrive_check_scopes(Gdrive_Info* pInfo);
char* _gdrive_get_root_folder_id(Gdrive_Info* pInfo);
char* _gdrive_get_child_id_by_name(Gdrive_Info* pInfo, 
                                   const char* parentId, 
                                   const char* childName
);




void _gdrive_exponential_wait(int tryNum);

struct curl_slist* _gdrive_authbearer_header(
        Gdrive_Info_Internal* pInternalInfo, struct curl_slist* pHeaders
);
enum Gdrive_Retry_Method _gdrive_retry_on_error(Gdrive_Download_Buffer* pBuf, 
                                                long httpResp
);
Gdrive_Download_Buffer* _gdrive_do_transfer(
        Gdrive_Info* pInfo, enum Gdrive_Request_Type requestType,
        bool retryOnAuthError, const char* url,  const Gdrive_Query* pQuery, 
        struct curl_slist* pHeaders, FILE* destFile
);
int _gdrive_download_to_buffer_with_retry(Gdrive_Info* pInfo, 
                                          Gdrive_Download_Buffer* pBuf /*, 
                                          long* pHttpResp */, 
                                          bool retryOnAuthError,
                                          int tryNum,
                                          int maxTries
);

Gdrive_Fileinfo* _gdrive_check_cache(Gdrive_Cache_Node* pRoot, 
                                     const char* fileId
);







long _gdrive_divide_round_up(long dividend, long divisor);






#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_INTERNAL_H */

