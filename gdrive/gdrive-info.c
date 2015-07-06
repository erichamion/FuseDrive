

#include "gdrive-info.h"
#include "gdrive-cache.h"

#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include "gdrive-client-secret.h"


/*************************************************************************
 * Constants needed only internally within this file
 *************************************************************************/

#define GDRIVE_REDIRECT_URI "urn:ietf:wg:oauth:2.0:oob"

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
// GDRIVE_URL_FILES, GDRIVE_URL_ABOUT, and GDRIVE_URL_CHANGES defined in 
// gdrive-internal.h because they are used elsewhere

#define GDRIVE_SCOPE_META "https://www.googleapis.com/auth/drive.readonly.metadata"
#define GDRIVE_SCOPE_READ "https://www.googleapis.com/auth/drive.readonly"
#define GDRIVE_SCOPE_WRITE "https://www.googleapis.com/auth/drive"
#define GDRIVE_SCOPE_APPS "https://www.googleapis.com/auth/drive.apps.readonly"
#define GDRIVE_SCOPE_MAXLENGTH 200

#define GDRIVE_RETRY_LIMIT 5


#define GDRIVE_ACCESS_MODE_COUNT 4
static const int GDRIVE_ACCESS_MODES[] = {GDRIVE_ACCESS_META,
                                   GDRIVE_ACCESS_READ,
                                   GDRIVE_ACCESS_WRITE,
                                   GDRIVE_ACCESS_APPS
};
static const char* const GDRIVE_ACCESS_SCOPES[] = {GDRIVE_SCOPE_META, 
                                  GDRIVE_SCOPE_READ, 
                                  GDRIVE_SCOPE_WRITE,
                                  GDRIVE_SCOPE_APPS 
                                  };


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_Info
{
    // Global, publicly accessible settings
    size_t minChunkSize;
    int maxChunks;
    
    // Members from here on are only for use within Gdrive code/header files.
    int mode;
    bool userInteractionAllowed;
    char* authFilename;
    char* accessToken;
    char* refreshToken;
    long accessTokenLength;  // Number of bytes allocated for accessToken
    long refreshTokenLength; // Number of bytes allocated for refreshToken
    const char* clientId;
    const char* clientSecret;
    const char* redirectUri;
    bool isCurlInitialized;
    CURL* curlHandle;
} Gdrive_Info;




static int gdrive_read_auth_file(const char* filename);

static void gdrive_info_cleanup(void);

static int 
gdrive_refresh_auth_token(const char* grantType, const char* tokenString);

static int gdrive_prompt_for_auth(void);

static int gdrive_check_scopes(void);

static char* gdrive_get_root_folder_id(void);

static char* 
gdrive_get_child_id_by_name(const char* parentId, const char* childName);

static int gdrive_save_auth(void);

void gdrive_curlhandle_setup(CURL* curlHandle);




/*************************************************************************
 * Implementations of fully public functions intended for use outside of
 * gdrive-* files (as well as inside)
 *************************************************************************/

/******************
 * Fully public constructors, factory methods, destructors and similar
 ******************/

/*
 * gdrive_init():   Initializes the network connection, sets appropriate 
 *                  settings for the Google Drive session, and ensures the user 
 *                  has granted necessary access permissions for the Google 
 *                  Drive account.
 */
int gdrive_init(int access, 
                const char* authFilename, 
                time_t cacheTTL,
                enum Gdrive_Interaction interactionMode,
                size_t minFileChunkSize,
                int maxChunksPerFile
)
{
    Gdrive_Info* pInfo = gdrive_get_info();
    
    // If necessary, initialize curl.
    if (!(pInfo->isCurlInitialized) && 
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
    pInfo->isCurlInitialized = true;
    
    return gdrive_init_nocurl(access, 
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
int gdrive_init_nocurl(int access, 
                       const char* authFilename, 
                       time_t cacheTTL,
                       enum Gdrive_Interaction interactionMode,
                       size_t minFileChunkSize,
                       int maxChunksPerFile
)
{
    // Seed the RNG.
    srand(time(NULL));
    
    Gdrive_Info* pInfo = gdrive_get_info();
    // Assume curl_global_init() has already been called somewhere.
    pInfo->isCurlInitialized = true;
    
    // Set up the Google Drive client ID and secret.
    pInfo->clientId = GDRIVE_CLIENT_ID;
    pInfo->clientSecret = GDRIVE_CLIENT_SECRET;
    pInfo->redirectUri = GDRIVE_REDIRECT_URI;
    
    // Can we prompt the user for authentication during initial setup?
    if (interactionMode == GDRIVE_INTERACTION_STARTUP || 
            interactionMode == GDRIVE_INTERACTION_ALWAYS)
    {
        pInfo->userInteractionAllowed = true;
    }
    
    // If a filename was given, attempt to open the file and read its contents.
    if (authFilename != NULL)
    {
        pInfo->authFilename = malloc(strlen(authFilename) + 1);
        if (pInfo->authFilename != NULL)
        {
            strcpy(pInfo->authFilename, authFilename);
            gdrive_read_auth_file(authFilename);
        }
    }
        
    // Authenticate or refresh access
    pInfo->mode = access;
    if (pInfo->mode & GDRIVE_ACCESS_WRITE) 
    {
        // Write access implies read access
        pInfo->mode = pInfo->mode | GDRIVE_ACCESS_READ;
    }
    if (pInfo->mode & GDRIVE_ACCESS_READ) 
    {
        // Read access implies meta-info access
        pInfo->mode = pInfo->mode | GDRIVE_ACCESS_META;
    }
    if (gdrive_auth() != 0)
    {
        // Could not get the required permissions.  Return error.
        return -1;
    }
    gdrive_save_auth();
    // Can we continue prompting for authentication if needed later?
    pInfo->userInteractionAllowed = 
            (interactionMode == GDRIVE_INTERACTION_ALWAYS);
    
    // Initialize the cache
    if (gdrive_cache_init(cacheTTL) != 0)
    {
        // Cache initialization error, probably a memory error
        return -1;
    }
    //pInfo->settings.cacheTTL = cacheTTL;
    
    // Set chunk size
    pInfo->minChunkSize = (minFileChunkSize > 0) ?
        gdrive_divide_round_up(minFileChunkSize, GDRIVE_BASE_CHUNK_SIZE) * 
            GDRIVE_BASE_CHUNK_SIZE :
        GDRIVE_BASE_CHUNK_SIZE;
    pInfo->maxChunks = maxChunksPerFile;
    
    return 0;
}

void gdrive_cleanup(void)
{
    gdrive_cleanup_nocurl();
    curl_global_cleanup();
}

void gdrive_cleanup_nocurl(void)
{
    gdrive_sysinfo_cleanup();
    gdrive_cache_cleanup();
    gdrive_info_cleanup();
}



/******************
 * Fully public getter and setter functions
 ******************/

size_t gdrive_get_minchunksize(void)
{
    return gdrive_get_info()->minChunkSize;
}

int gdrive_get_maxchunks(void)
{
    return gdrive_get_info()->maxChunks;
}

int gdrive_get_filesystem_perms(enum Gdrive_Filetype type)
{
    // Get the permissions for regular files.
    int perms = 0;
    const Gdrive_Info* pInfo = gdrive_get_info();
    if (pInfo->mode & GDRIVE_ACCESS_READ)
    {
        // Read access
        perms = perms | S_IROTH;
    }
    if (pInfo->mode & GDRIVE_ACCESS_WRITE)
    {
        // Write access
        perms = perms | S_IWOTH;
    }
    // No execute access (at least for regular files)
    
    // Folders always need read and execute access.
    if (type == GDRIVE_FILETYPE_FOLDER)
    {
        perms = perms | S_IROTH | S_IXOTH;
    }
    
    return perms;
}



/******************
 * Other fully public functions
 ******************/

/*
 * The path argument should be an absolute path, starting with '/'. Can be 
 * either a file or a directory (folder).
 */
char* gdrive_filepath_to_id(const char* path)
{
    //char* result = NULL;
    if (path == NULL || (path[0] != '/'))
    {
        // Invalid path
        return NULL;
    }
    
    // Try to get the ID from the cache.
    char* cachedId = gdrive_cache_get_fileid(path);
    if (cachedId != NULL)
    {
        return cachedId;
    }
    // else ID isn't in the cache yet
    
    // Is this the root folder?
    char* result = NULL;
    if (strcmp(path, "/") == 0)
    {
        result = gdrive_get_root_folder_id();
        if (result != NULL)
        {
            // Add to the fileId cache.
            gdrive_cache_add_fileid(path, result);
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
    char* parentId = gdrive_filepath_to_id(parentPath);
    free(parentPath);
    if (parentId == NULL)
    {
        // An error occurred.
        return NULL;
    }
    // Use the parent's ID to find the child's ID.
    char* id = gdrive_get_child_id_by_name(parentId, path + index + 1);
    free(parentId);
    
    // Add the ID to the fileId cache.
    if (id != NULL)
    {
        // Don't just return the ID directly, because that would cause a memory
        // leak. We need to return a pointer to const that does not need freed
        // by the caller.
        int success = gdrive_cache_add_fileid(path, id);
        free(id);
        if (success == 0)
        {
            return gdrive_cache_get_fileid(path);
        }
        else
        {
            return NULL;
        }
    }
    // else error
    return NULL;
}

Gdrive_Fileinfo_Array* gdrive_folder_list(const char* folderId)
{
    // Allow for an initial quote character in addition to the terminating null
    char* filter = malloc(strlen(folderId) + 
                            strlen("' in parents and trashed=false") + 2);
    if (filter == NULL)
    {
        return NULL;
    }
    strcpy(filter, "'");
    strcat(filter, folderId);
    strcat(filter, "' in parents and trashed=false");
    
    // Prepare the network request
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_GET);
    
    if (
            gdrive_xfer_set_url(pTransfer, GDRIVE_URL_FILES) ||
            gdrive_xfer_add_query(pTransfer, "q", filter) ||
            gdrive_xfer_add_query(pTransfer, "fields", 
                                  "items(title,id,mimeType)")
        )
    {
        // Error
        free(filter);
        gdrive_xfer_free(pTransfer);
        return NULL;
    }
    free(filter);
    
    // Send the network request
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    
    // TODO: Somehow unify this process with other ways to fill Gdrive_Fileinfo,
    // reducing code duplication and taking advantage of the cache.
    int fileCount = -1;
    Gdrive_Fileinfo_Array* pArray = NULL;
    if (pBuf != NULL && gdrive_dlbuf_get_httpresp(pBuf) < 400)
    {
        // Transfer was successful.  Convert result to a JSON object and extract
        // the file meta-info.
        Gdrive_Json_Object* pObj = 
                gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
        if (pObj != NULL)
        {
            fileCount = gdrive_json_array_length(pObj, "items");
            if (fileCount > 0)
            {
                // Create an array of Gdrive_Fileinfo structs large enough to
                // hold all the items.
                pArray = gdrive_finfoarray_create(fileCount);
                if (pArray != NULL)
                {
                    // Extract the file info from each member of the array.
                    for (int index = 0; index < fileCount; index++)
                    {
                        Gdrive_Json_Object* pFile = gdrive_json_array_get(
                                pObj, 
                                "items", 
                                index
                                );
                        if (pFile != NULL)
                        {
                            gdrive_finfoarray_add_from_json(pArray, pFile);
                        }
                    }
                }
                else
                {
                    // Memory error.
                    fileCount = -1;
                }
            }
            // else either failure (return -1) or 0-length array (return 0),
            // nothing special needs to be done.
            
            gdrive_json_kill(pObj);
        }
        // else do nothing.  Already prepared to return error.
    }
    
    gdrive_dlbuf_free(pBuf);

    return pArray;
}

int gdrive_remove_parent(const char* fileId, const char* parentId)
{
    assert(fileId != NULL && fileId[0] != '\0' && 
            parentId != NULL && parentId[0] != '\0'
            );
    
    // URL will look like 
    // "<standard Drive Files url>/<fileId>/parents/<parentId>"
    char* url = malloc(strlen(GDRIVE_URL_FILES) + 1 + strlen(fileId) + 
        strlen("/parents/") + strlen(parentId) + 1);
    if (url == NULL)
    {
        // Memory error
        return -ENOMEM;
    }
    strcpy(url, GDRIVE_URL_FILES);
    strcat(url, "/");
    strcat(url, fileId);
    strcat(url, "/parents/");
    strcat(url, parentId);
    
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        free(url);
        return -ENOMEM;
    }
    if (gdrive_xfer_set_url(pTransfer, url) != 0)
    {
        // Error, probably memory
        gdrive_xfer_free(pTransfer);
        free(url);
        return -ENOMEM;
    }
    free(url);
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_DELETE);
    
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    int returnVal = (pBuf == NULL || gdrive_dlbuf_get_httpresp(pBuf) >= 400) ? 
        -EIO : 0;
    gdrive_dlbuf_free(pBuf);
    return returnVal;
}

int gdrive_delete(const char* fileId, const char* parentId)
{
    // TODO: If support for manipulating trashed files is added, we'll need to
    // check whether the specified file is already trashed, and permanently 
    // delete if it is.
    // TODO: May want to add an option for whether to trash or really delete.
    
    assert(fileId != NULL && fileId[0] != '\0');
    
    // URL will look like 
    // "<standard Drive Files url>/<fileId>/trash"
    char* url = malloc(strlen(GDRIVE_URL_FILES) + 1 + strlen(fileId) + 
        strlen("/trash") + 1);
    if (url == NULL)
    {
        // Memory error
        return -ENOMEM;
    }
    strcpy(url, GDRIVE_URL_FILES);
    strcat(url, "/");
    strcat(url, fileId);
    strcat(url, "/trash");
    
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        free(url);
        return -ENOMEM;
    }
    if (gdrive_xfer_set_url(pTransfer, url) != 0)
    {
        // Error, probably memory
        gdrive_xfer_free(pTransfer);
        free(url);
        return -ENOMEM;
    }
    free(url);
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_POST);
    
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    int returnVal = (pBuf == NULL || gdrive_dlbuf_get_httpresp(pBuf) >= 400) ? 
        -EIO : 0;
    gdrive_dlbuf_free(pBuf);
    if (returnVal == 0)
    {
        gdrive_cache_delete_id(fileId);
        if (parentId != NULL && strcmp(parentId, "/") != 0)
        {
            // Remove the parent from the cache because the child count will be
            // wrong.
            gdrive_cache_delete_id(parentId);
        }
    }
    return returnVal;
}

int gdrive_add_parent(const char* fileId, const char* parentId)
{
    assert(fileId != NULL && fileId[0] != '\0' && 
            parentId != NULL && parentId[0] != '\0'
            );
    
    // URL will look like 
    // "<standard Drive Files url>/<fileId>/parents"
    char* url = malloc(strlen(GDRIVE_URL_FILES) + 1 + strlen(fileId) + 1 +
        strlen("/parents") + 1);
    if (url == NULL)
    {
        // Memory error
        return -ENOMEM;
    }
    strcpy(url, GDRIVE_URL_FILES);
    strcat(url, "/");
    strcat(url, fileId);
    strcat(url, "/parents");
    
    // Create the Parent resource for the request body
    Gdrive_Json_Object* pObj = gdrive_json_new();
    if (!pObj)
    {
        // Memory error
        free(url);
        return -ENOMEM;
    }
    gdrive_json_add_string(pObj, "id", parentId);
    char* body = gdrive_json_to_new_string(pObj, false);
    gdrive_json_kill(pObj);
    if (!body)
    {
        // Memory error
        free(url);
        return -ENOMEM;
    }
    
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        free(url);
        free(body);
        return -ENOMEM;
    }
    if (gdrive_xfer_set_url(pTransfer, url) || 
            gdrive_xfer_add_header(pTransfer, "Content-Type: application/json")
            )
    {
        // Error, probably memory
        gdrive_xfer_free(pTransfer);
        free(url);
        free(body);
        return -ENOMEM;
    }
    free(url);
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_POST);
    gdrive_xfer_set_body(pTransfer, body);
    
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    free(body);
    
    int returnVal = (pBuf == NULL || gdrive_dlbuf_get_httpresp(pBuf) >= 400) ? 
        -EIO : 0;
    gdrive_dlbuf_free(pBuf);
    
    if (returnVal == 0)
    {
        // Update the parent count in case we do anything else with the file
        // before the cache expires. (For example, if there was only one parent
        // before, and the user deletes one of the links, we don't want to
        // delete the entire file because of a bad parent count).
        Gdrive_Fileinfo* pFileinfo = gdrive_cache_get_item(fileId, false, NULL);
        if (pFileinfo)
        {
            pFileinfo->nParents++;
        }
    }
    return returnVal;
}

int gdrive_change_basename(const char* fileId, const char* newName)
{
    assert(fileId && newName && fileId[0] != '\0' && newName[0] != '\0');
    
    // Create the request body with the new name
    Gdrive_Json_Object* pObj = gdrive_json_new();
    if (!pObj)
    {
        // Memory error
        return -ENOMEM;
    }
    gdrive_json_add_string(pObj, "title", newName);
    char* body = gdrive_json_to_new_string(pObj, false);
    gdrive_json_kill(pObj);
    if (!body)
    {
        // Error, probably memory
        return -ENOMEM;
    }
    
    // Create the url in the form of:
    // "<GDRIVE_URL_FILES>/<fileId>"
    char* url = malloc(strlen(GDRIVE_URL_FILES) + 1 + strlen(fileId) + 1);
    if (!url)
    {
        // Memory error
        free(body);
        return -ENOMEM;
    }
    strcpy(url, GDRIVE_URL_FILES);
    strcat(url, "/");
    strcat(url, fileId);
    
    // Set up the network transfer
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (!pTransfer)
    {
        // Memory error
        free(url);
        free(body);
        return -ENOMEM;
    }
    if (gdrive_xfer_set_url(pTransfer, url) || 
            gdrive_xfer_add_query(pTransfer, "updateViewedDate", "false") ||
            gdrive_xfer_add_header(pTransfer, "Content-Type: application/json")
            )
    {
        // Error, probably memory
        gdrive_xfer_free(pTransfer);
        free(url);
        free(body);
        return -ENOMEM;
    }
    free(url);
    gdrive_xfer_set_body(pTransfer, body);
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_PATCH);
    
    // Send the network request
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    free(body);
    
    int returnVal = (pBuf == NULL || gdrive_dlbuf_get_httpresp(pBuf) >= 400) ? 
        -EIO : 0;
    gdrive_dlbuf_free(pBuf);
    return returnVal;
    
}


/*************************************************************************
 * Implementations of semi-public functions - for public use within any
 * gdrive-* file, but not intended for use outside gdrive-* files
 *************************************************************************/

/******************
 * Semi-public constructors, factory methods, destructors and similar
 ******************/

Gdrive_Info* gdrive_get_info(void)
{
    static Gdrive_Info info = {0};
    return &info;
}



/******************
 * Semi-public getter and setter functions
 ******************/

CURL* gdrive_get_curlhandle(void)
{
    Gdrive_Info* pInfo = gdrive_get_info();
    if (pInfo->curlHandle == NULL)
    {
        pInfo->curlHandle = curl_easy_init();
        if (pInfo->curlHandle == NULL)
        {
            // Error
            return NULL;
        }
        gdrive_curlhandle_setup(pInfo->curlHandle);
    }
    return curl_easy_duphandle(pInfo->curlHandle);
}

const char* gdrive_get_access_token(void)
{
    return gdrive_get_info()->accessToken;
}


/******************
 * Other semi-public accessible functions
 ******************/

int gdrive_auth(void)
{
    Gdrive_Info* pInfo = gdrive_get_info();
    
    // Try to refresh existing tokens first.
    if (pInfo->refreshToken != NULL && pInfo->refreshToken[0] != '\0')
    {
        int refreshSuccess = gdrive_refresh_auth_token(
                GDRIVE_GRANTTYPE_REFRESH,
                pInfo->refreshToken
        );
        
        if (refreshSuccess == 0)
        {
            // Refresh succeeded, but we don't know what scopes were previously
            // granted.  Check to make sure we have the required scopes.  If so,
            // then we don't need to do anything else and can return success.
            int success = gdrive_check_scopes();
            if (success == 0)
            {
                // Refresh succeeded with correct scopes, return success.
                return 0;
            }
        }
    }
    
    // Either didn't have a refresh token, or it didn't work.  Need to get new
    // authorization, if allowed.
    if (!pInfo->userInteractionAllowed)
    {
        // Need to get new authorization, but not allowed to interact with the
        // user.  Return error.
        return -1;
    }
    
    // If we've gotten this far, then we need to interact with the user, and
    // we're allowed to do so.  Prompt for authorization, and return whatever
    // success or failure the prompt returns.
    return gdrive_prompt_for_auth();
}








/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

static int gdrive_read_auth_file(const char* filename)
{
    if (filename == NULL)
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
        
        Gdrive_Json_Object* pObj = gdrive_json_from_string(buffer);
        if (pObj == NULL)
        {
            // Couldn't convert the file contents to a JSON object, prepare to
            // return failure.
            returnVal = -1;
        }
        else
        {
            Gdrive_Info* pInfo = gdrive_get_info();
            pInfo->accessToken = gdrive_json_get_new_string(
                    pObj, 
                    GDRIVE_FIELDNAME_ACCESSTOKEN, 
                    &(pInfo->accessTokenLength)
                    );
            pInfo->refreshToken = gdrive_json_get_new_string(
                    pObj, 
                    GDRIVE_FIELDNAME_REFRESHTOKEN, 
                    &(pInfo->refreshTokenLength)
                    );
            
            if ((pInfo->accessToken == NULL) || (pInfo->refreshToken == NULL))
            {
                // Didn't get one or more auth tokens from the file.
                returnVal = -1;
            }
            gdrive_json_kill(pObj);
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

static void gdrive_info_cleanup(void)
{
    Gdrive_Info* pInfo = gdrive_get_info();
    
    pInfo->minChunkSize = 0;
    pInfo->maxChunks = 0;
    
    pInfo->mode = 0;
    pInfo->userInteractionAllowed = false;
    
    free(pInfo->authFilename);
    pInfo->authFilename = NULL;
    
    free(pInfo->accessToken);
    pInfo->accessToken = NULL;
    pInfo->accessTokenLength = 0;
    
    free(pInfo->refreshToken);
    pInfo->refreshToken = NULL;
    pInfo->refreshTokenLength = 0;
    
    pInfo->clientId = NULL;
    pInfo->clientSecret = NULL;
    pInfo->redirectUri = NULL;
    

    if (pInfo->curlHandle != NULL)
    {
        curl_easy_cleanup(pInfo->curlHandle);
        pInfo->curlHandle = NULL;
    }
}



static int 
gdrive_refresh_auth_token(const char* grantType, const char* tokenString)
{
    // Make sure we were given a valid grant_type
    if (strcmp(grantType, GDRIVE_GRANTTYPE_CODE) && 
            strcmp(grantType, GDRIVE_GRANTTYPE_REFRESH))
    {
        // Invalid grant_type
        return -1;
    }
    
    Gdrive_Info* pInfo = gdrive_get_info();
    
    // Prepare the network request
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        // Memory error
        return -1;
    }
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_POST);
    
    // We're trying to get authorization, so it doesn't make sense to retry if
    // authentication fails.
    gdrive_xfer_set_retryonautherror(pTransfer, false);
    
    // Set up the post data. Some of the post fields depend on whether we have
    // an auth code or a refresh token, and some do not.
    const char* tokenOrCodeField = NULL;
    if (strcmp(grantType, GDRIVE_GRANTTYPE_CODE) == 0)
    {
        // Converting an auth code into auth and refresh tokens.  Interpret
        // tokenString as the auth code.
        if (gdrive_xfer_add_postfield(pTransfer,
                                  GDRIVE_FIELDNAME_REDIRECTURI, 
                                  GDRIVE_REDIRECT_URI
                ) != 0)
        {
            // Error
            gdrive_xfer_free(pTransfer);
            return -1;
        }
        tokenOrCodeField = GDRIVE_FIELDNAME_CODE;
    }
    else
    {
        // Refreshing an existing refresh token.  Interpret tokenString as the
        // refresh token.
        tokenOrCodeField = GDRIVE_FIELDNAME_REFRESHTOKEN;
    }
    if (
            gdrive_xfer_add_postfield(pTransfer, tokenOrCodeField, 
                                      tokenString) ||
            gdrive_xfer_add_postfield(pTransfer, GDRIVE_FIELDNAME_CLIENTID, 
                                      GDRIVE_CLIENT_ID) ||
            gdrive_xfer_add_postfield(pTransfer, GDRIVE_FIELDNAME_CLIENTSECRET,
                                      GDRIVE_CLIENT_SECRET) ||
            gdrive_xfer_add_postfield(pTransfer, GDRIVE_FIELDNAME_GRANTTYPE, 
                                      grantType) ||
            gdrive_xfer_set_url(pTransfer, GDRIVE_URL_AUTH_TOKEN)
        )
    {
        // Error
        gdrive_xfer_free(pTransfer);
        return -1;
    }
        
    // Do the transfer. 
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    if (pBuf == NULL)
    {
        // There was an error sending the request and getting the response.
        return -1;
    }
    if (gdrive_dlbuf_get_httpresp(pBuf) >= 400)
    {
        // Failure, but probably not an error.  Most likely, the user has
        // revoked permission or the refresh token has otherwise been
        // invalidated.
        gdrive_dlbuf_free(pBuf);
        return 1;
    }
    
    // If we've gotten this far, we have a good HTTP response.  Now we just
    // need to pull the access_token string (and refresh token string if
    // present) out of it.

    Gdrive_Json_Object* pObj = gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
    gdrive_dlbuf_free(pBuf);
    if (pObj == NULL)
    {
        // Couldn't locate JSON-formatted information in the server's 
        // response.  Return error.
        return -1;
    }
    int returnVal = gdrive_json_realloc_string(
            pObj, 
            GDRIVE_FIELDNAME_ACCESSTOKEN,
            &(pInfo->accessToken),
            &(pInfo->accessTokenLength)
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
            gdrive_json_realloc_string(
                    pObj, 
                    GDRIVE_FIELDNAME_REFRESHTOKEN,
                    &(pInfo->refreshToken),
                    &(pInfo->refreshTokenLength)
                    );
        }
    }
    gdrive_json_kill(pObj);
    
    return returnVal;
}

static int gdrive_prompt_for_auth(void)
{
    Gdrive_Info* pInfo = gdrive_get_info();
    
    char scopeStr[GDRIVE_SCOPE_MAXLENGTH] = "";
    bool scopeFound = false;
    
    // Check each of the possible permissions, and add the appropriate scope
    // if necessary.
    for (int i = 0; i < GDRIVE_ACCESS_MODE_COUNT; i++)
    {
        if (pInfo->mode & GDRIVE_ACCESS_MODES[i])
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
    
    Gdrive_Query* pQuery = NULL;
    pQuery = gdrive_query_add(pQuery, "response_type", "code");
    pQuery = gdrive_query_add(pQuery, "client_id", GDRIVE_CLIENT_ID);
    pQuery = gdrive_query_add(pQuery, "redirect_uri", GDRIVE_REDIRECT_URI);
    pQuery = gdrive_query_add(pQuery, "scope", scopeStr);
    pQuery = gdrive_query_add(pQuery, "include_granted_scopes", "true");
    if (pQuery == NULL)
    {
        // Memory error
        return -1;
    }
    
    char* authUrl = gdrive_query_assemble(pQuery, GDRIVE_URL_AUTH_NEWAUTH);
    gdrive_query_free(pQuery);
    
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
    if (!fgets(authCode, 1024, stdin))
    {
        puts("Error getting user input");
        return -1;
    }
    
    if (authCode[0] == '\0')
    {
        // No code entered, return failure.
        return 1;
    }
    
    // Exchange the authorization code for access and refresh tokens.
    return gdrive_refresh_auth_token(GDRIVE_GRANTTYPE_CODE, authCode);
}

static int gdrive_check_scopes(void)
{
    Gdrive_Info* pInfo = gdrive_get_info();
    
    // Prepare the network request
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        // Memory error
        return -1;
    }
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_GET);
    gdrive_xfer_set_retryonautherror(pTransfer, false);
    if (
            gdrive_xfer_set_url(pTransfer, GDRIVE_URL_AUTH_TOKENINFO) ||
            gdrive_xfer_add_query(pTransfer, GDRIVE_FIELDNAME_ACCESSTOKEN, 
                                  pInfo->accessToken)
        )
    {
        // Error
        gdrive_xfer_free(pTransfer);
        return -1;
    }
    
    // Send the network request
    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    if (pBuf == NULL || gdrive_dlbuf_get_httpresp(pBuf) >= 400)
    {
        // Download failed or gave a bad response.
        gdrive_dlbuf_free(pBuf);
        return -1;
    }
    
    // If we've made it this far, we have an ok response.  Extract the scopes
    // from the JSON array that should have been returned, and compare them
    // with the expected scopes.
    
    Gdrive_Json_Object* pObj = gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
    gdrive_dlbuf_free(pBuf);
    if (pObj == NULL)
    {
        // Couldn't interpret the response as JSON, return error.
        return -1;
    }
    char* grantedScopes = gdrive_json_get_new_string(pObj, "scope", NULL);
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
    long scopeLength = strlen(grantedScopes);
    int matchedScopes = 0;
    while (startIndex <= scopeLength)
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
        if ((pInfo->mode & GDRIVE_ACCESS_MODES[i]) && 
                !(matchedScopes & GDRIVE_ACCESS_MODES[i])
                )
        {
            return -1;
        }
    }
    
    // If we made it through to here, return success.
    return 0;
}

static char* gdrive_get_root_folder_id(void)
{
    const char* mainCopy = gdrive_sysinfo_get_rootid();
    char* newCopy = malloc(strlen(mainCopy) + 1);
    if (newCopy)
        strcpy(newCopy, mainCopy);
    return newCopy;
}

static char* 
gdrive_get_child_id_by_name(const char* parentId, const char* childName)
{
    // Construct a filter in the form of 
    // "'<parentId>' in parents and title = '<childName>'"
    char* filter = 
        malloc(strlen("'' in parents and title = '' and trashed = false") + 
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
    strcat(filter, "' and trashed = false");
    
    Gdrive_Transfer* pTransfer = gdrive_xfer_create();
    if (pTransfer == NULL)
    {
        // Memory error
        free(filter);
        return NULL;
    }
    gdrive_xfer_set_requesttype(pTransfer, GDRIVE_REQUEST_GET);
    if (
            gdrive_xfer_set_url(pTransfer, GDRIVE_URL_FILES) ||
            gdrive_xfer_add_query(pTransfer, "q", filter) ||
            gdrive_xfer_add_query(pTransfer, "fields", "items(id)")
        )
    {
        // Error
        free(filter);
        gdrive_xfer_free(pTransfer);
        return NULL;
    }
    free(filter);

    Gdrive_Download_Buffer* pBuf = gdrive_xfer_execute(pTransfer);
    gdrive_xfer_free(pTransfer);
    
    if (pBuf == NULL || gdrive_dlbuf_get_httpresp(pBuf) >= 400)
    {
        // Download error
        gdrive_dlbuf_free(pBuf);
        return NULL;
    }
    
    
    // If we're here, we have a good response.  Extract the ID from the 
    // response.
    
    // Convert to a JSON object.
    Gdrive_Json_Object* pObj = 
            gdrive_json_from_string(gdrive_dlbuf_get_data(pBuf));
    gdrive_dlbuf_free(pBuf);
    if (pObj == NULL)
    {
        // Couldn't convert to JSON object.
        return NULL;
    }
    
    char* id = NULL;
    Gdrive_Json_Object* pArrayItem = gdrive_json_array_get(pObj, "items", 0);
    if (pArrayItem != NULL)
    {
        id = gdrive_json_get_new_string(pArrayItem, "id", NULL);
    }
    gdrive_json_kill(pObj);
    return id;
}

static int gdrive_save_auth(void)
{
    Gdrive_Info* pInfo = gdrive_get_info();
    
    if (pInfo->authFilename == NULL || 
            pInfo->authFilename[0] == '\0')
    {
        // Do nothing if there's no filename
        return -1;
    }
    
    // Create a JSON object, fill it with the necessary details, 
    // convert to a string, and write to the file.
    FILE* outFile = gdrive_power_fopen(pInfo->authFilename, "w");
    if (outFile == NULL)
    {
        // Couldn't open file for writing.
        return -1;
    }
    
    Gdrive_Json_Object* pObj = gdrive_json_new();
    gdrive_json_add_string(pObj, GDRIVE_FIELDNAME_ACCESSTOKEN, 
                           pInfo->accessToken
            );
    gdrive_json_add_string(pObj, GDRIVE_FIELDNAME_REFRESHTOKEN, 
                           pInfo->refreshToken
            );
    int success = fputs(gdrive_json_to_string(pObj, true), outFile);
    gdrive_json_kill(pObj);
    fclose(outFile);
    
    return (success >= 0) ? 0 : -1;
}

void gdrive_curlhandle_setup(CURL* curlHandle)
{
    // Accept compressed responses and let libcurl automatically uncompress
    curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, "");
    
    // Automatically follow redirects
    curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1);
}