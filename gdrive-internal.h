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

#define GDRIVE_ACCESS_MODE_COUNT 4
const int GDRIVE_ACCESS_MODES[] = {GDRIVE_ACCESS_META,
                                   GDRIVE_ACCESS_READ,
                                   GDRIVE_ACCESS_WRITE,
                                   GDRIVE_ACCESS_APPS
};
const char* const GDRIVE_ACCESS_SCOPES[] = {GDRIVE_SCOPE_META, 
                                  GDRIVE_SCOPE_READ, 
                                  GDRIVE_SCOPE_WRITE,
                                  GDRIVE_SCOPE_APPS 
                                  };
    
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

typedef struct Gdrive_File_Contents
{
    off_t start;
    off_t end;
    FILE* fh;
    struct Gdrive_File_Contents* pNext;
} Gdrive_File_Contents;

typedef struct Gdrive_Cache_Node
{
    time_t lastUpdateTime;
    int openReads;
    int openWrites;
    int openOthers;
    Gdrive_Fileinfo fileinfo;
    Gdrive_File_Contents* pContents;
    struct Gdrive_Cache_Node* pParent;
    struct Gdrive_Cache_Node* pLeft;
    struct Gdrive_Cache_Node* pRight;
} Gdrive_Cache_Node;

typedef struct Gdrive_Fileid_Cache_Node
{
    time_t lastUpdateTime;
    char* path;
    char* fileId;
    struct Gdrive_Fileid_Cache_Node* pNext;
} Gdrive_Fileid_Cache_Node;

typedef struct Gdrive_Cache
{
    time_t lastUpdateTime;
    int64_t nextChangeId;
    Gdrive_Cache_Node* pCacheHead;
    Gdrive_Fileid_Cache_Node fileIdCacheHead;  // Intentionally not a pointer.
} Gdrive_Cache;

typedef struct Gdrive_Query
{
    CURL* curlHandle;
    char* field;
    char* value;
    struct Gdrive_Query* pNext;
} Gdrive_Query;


typedef struct Gdrive_Info_Internal
{
    char* accessToken;
    char* refreshToken;
    long accessTokenLength;  // Number of bytes allocated for accessToken
    long refreshTokenLength; // Number of bytes allocated for refreshToken
    char* clientId;
    char* clientSecret;
    char* redirectUri;
    bool isCurlInitialized;
    CURL* curlHandle;
    Gdrive_Sysinfo* pSysinfo;
    Gdrive_Cache cache;
} Gdrive_Info_Internal;

typedef struct Gdrive_Download_Buffer
{
    size_t allocatedSize;
    size_t usedSize;
    long httpResp;
    CURLcode resultCode;
    char* data;
    FILE* fh;
} Gdrive_Download_Buffer;





int _gdrive_info_create(Gdrive_Info** ppInfo);
int _gdrive_info_internal_create(Gdrive_Info_Internal** ppInfo);
int _gdrive_read_auth_file(const char* filename, Gdrive_Info_Internal* pInfo);
void _gdrive_info_free(Gdrive_Info* pInfo);
void _gdrive_settings_cleanup(Gdrive_Settings* pSettings);
void _gdrive_info_internal_free(Gdrive_Info_Internal* pInfo);
int _gdrive_sysinfo_update(Gdrive_Info* pInfo, Gdrive_Sysinfo** ppDest);
int _gdrive_sysinfo_fill_from_json(Gdrive_Sysinfo* pDest, 
                                   gdrive_json_object* pObj
);
void _gdrive_sysinfo_cleanup(Gdrive_Sysinfo* pSysinfo);
CURLcode _gdrive_download_to_buffer(CURL* curlHandle, 
                                    Gdrive_Download_Buffer* pBuffer /*, 
                                    long* pHttpResp */,
                                    bool textMode
);
size_t _gdrive_download_buffer_callback_text(char *newData, 
                                             size_t size, 
                                             size_t nmemb, 
                                             void *userdata
);
size_t _gdrive_download_buffer_callback_bin(char *newData, 
                                            size_t size, 
                                            size_t nmemb, 
                                            void *userdata
);
size_t _gdrive_download_buffer_callback(char *newData, 
                                        size_t size, 
                                        size_t nmemb, 
                                        void *userdata,
                                        bool textMode
);
Gdrive_Download_Buffer* _gdrive_download_buffer_create(size_t initialSize);
void _gdrive_download_buffer_free(Gdrive_Download_Buffer* pBuf);
int _gdrive_refresh_auth_token(Gdrive_Info* pInfo, 
                               //Gdrive_Download_Buffer* pBuf,
                               const char* grantType,
                               const char* tokenString
);
int _gdrive_prompt_for_auth(Gdrive_Info* pInfo);
int _gdrive_check_scopes(//Gdrive_Download_Buffer* pBuf,
                         Gdrive_Info* pInfo
);
char* _gdrive_get_root_folder_id(Gdrive_Info* pInfo);
char* _gdrive_get_child_id_by_name(Gdrive_Info* pInfo, 
                                   const char* parentId, 
                                   const char* childName
);
size_t _gdrive_file_read_next_chunk(Gdrive_Info* pInfo, 
                                    Gdrive_Cache_Node* pNode,
                                    char* destBuf, 
                                    off_t offset, 
                                    size_t size
);
Gdrive_File_Contents* _gdrive_file_contents_find_chunk(
        Gdrive_File_Contents* pContents, 
        off_t offset
);
Gdrive_File_Contents* _gdrive_file_contents_create_chunk(
        Gdrive_Info* pInfo, Gdrive_Cache_Node* pNode, off_t offset, size_t size
);
Gdrive_File_Contents* _gdrive_file_contents_create(Gdrive_Cache_Node* pNode);
void _gdrive_file_contents_free(Gdrive_Cache_Node* pNode, 
                                Gdrive_File_Contents* pContents
);
void _gdrive_file_contents_free_all(Gdrive_File_Contents** ppContents);
int _gdrive_file_contents_fill_chunk(Gdrive_Info* pInfo,
                                     Gdrive_Fileinfo* pFileinfo, 
                                     Gdrive_File_Contents* pContents,
                                     off_t start, 
                                     size_t size
);
void _gdrive_exponential_wait(int tryNum);
char* _gdrive_new_string_from_json(gdrive_json_object* pObj, 
                                 const char* key,
                                 long* pLength
);
int _gdrive_realloc_string_from_json(gdrive_json_object* pObj, 
                                     const char* key,
                                     char** pDest, 
                                     long* pLength
);
struct curl_slist* _gdrive_authbearer_header(
        Gdrive_Info_Internal* pInternalInfo, struct curl_slist* pHeaders
);
enum Gdrive_Retry_Method _gdrive_retry_on_error(Gdrive_Download_Buffer* pBuf, 
                                                long httpResp
);

char* _gdrive_assemble_query_or_post(const char* url, 
                                     const Gdrive_Query* pQuery
);
Gdrive_Query* _gdrive_query_create(CURL* curlHandle);
int _gdrive_query_add(Gdrive_Query* pQuery, 
                      const char* field, 
                      const char* value
);
void _gdrive_query_free(Gdrive_Query* pQuery);
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
void _gdrive_get_fileinfo_from_json(Gdrive_Info* pInfo,
                                    gdrive_json_object* pObj, 
                                   Gdrive_Fileinfo* pFileinfo
);
Gdrive_Fileinfo* _gdrive_check_cache(Gdrive_Cache_Node* pRoot, 
                                     const char* fileId
);
Gdrive_Fileinfo* _gdrive_cache_get_item(Gdrive_Info* pInfo, 
                                        const char* fileId,
                                        bool addIfDoesntExist,
                                        bool* pAlreadyExists
);
Gdrive_Cache_Node* _gdrive_cache_get_node(Gdrive_Cache_Node* pParent,
                                          Gdrive_Cache_Node** ppNode,
                                          const char* fileId,
                                          bool addIfDoesntExist,
                                          bool* pAlreadyExists
);
void _gdrive_cache_delete_node(Gdrive_Cache_Node** ppFromParent, 
                               Gdrive_Cache_Node* pNode
);
void _gdrive_cache_node_swap(Gdrive_Cache_Node** ppFromParentOne,
                             Gdrive_Cache_Node* pNodeOne,
                             Gdrive_Cache_Node** ppFromParentTwo,
                             Gdrive_Cache_Node* pNodeTwo
);
Gdrive_Cache_Node* _gdrive_cache_node_create(Gdrive_Cache_Node* pParent);
void _gdrive_cache_node_free(Gdrive_Cache_Node* pNode);
Gdrive_Fileid_Cache_Node* _gdrive_fileid_cache_node_create(const char* filename,
                                                           const char* fileId);
int _gdrive_fileid_cache_update_item(Gdrive_Fileid_Cache_Node* pNode, 
                                     const char* fileId
);
const char* _gdrive_fileid_cache_get_item(Gdrive_Info* pInfo, 
                                          const char* path
);
Gdrive_Fileid_Cache_Node* _gdrive_fileid_cache_get_node(
        Gdrive_Fileid_Cache_Node* pHead, 
        const char* path
);
int _gdrive_fileid_cache_add_item(Gdrive_Fileid_Cache_Node* pHead, 
                                          const char* path,
                                          const char* fileId
);
void _gdrive_fileid_cache_node_free(Gdrive_Fileid_Cache_Node* pNode);
int _gdrive_cache_init(Gdrive_Info* pInfo);
int _gdrive_cache_update_if_stale(Gdrive_Info* pInfo);
int _gdrive_update_cache(Gdrive_Info* pInfo);
long _gdrive_divide_round_up(long dividend, long divisor);





#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_INTERNAL_H */

