/* 
 * File:   gdrive-info.h
 * Author: me
 * 
 * Declarations and definitions to be used by various gdrive code and header
 * files, but which are not part of the public gdrive interface.
 *
 * Created on April 14, 2015, 9:31 PM
 */

#ifndef GDRIVE_INFO_H
#define	GDRIVE_INFO_H

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
    
#include "gdrive-transfer.h"
#include "gdrive-util.h"
#include "gdrive-download-buffer.h"
#include "gdrive-query.h"
#include "gdrive.h"
    
#include <curl/curl.h>

    

    
#define GDRIVE_URL_FILES "https://www.googleapis.com/drive/v2/files"
#define GDRIVE_URL_ABOUT "https://www.googleapis.com/drive/v2/about"
#define GDRIVE_URL_CHANGES "https://www.googleapis.com/drive/v2/changes"
    

/******************
 * Semi-public constructors and destructors
 ******************/

Gdrive_Info* gdrive_get_info(void);



/******************
 * Semi-public getter and setter functions
 ******************/
    
/*
 * gdrive_get_curlhandle(): Retrieve the curl easy handle currently stored in 
 *                          the Gdrive_Info struct.
 * Return value (CURL*):
 *      A curl easy handle.
 */
CURL* gdrive_get_curlhandle(void);


/******************
 * Other semi-public accessible functions
 ******************/
    
/*
 * gdrive_auth():   Authenticate and obtain permissions from the user for Google
 *                  Drive.  If passed the address of a Gdrive_Info struct which 
 *                  has existing authentication information, will attempt to 
 *                  reuse this information first. The new credentials (if 
 *                  different from the credentials initially passed in) are
 *                  written back into the Gdrive_Info struct.
 * Returns:
 *      0 for success, other value on error.
 */
int gdrive_auth(void);
    
/*
 * gdrive_do_transfer():    Perform a download with the given information, 
 *                          automatically retrying as needed.
 * Parameters:
 *      requestType (enum Gdrive_Request_Type):
 *              The type of HTTP request (GET, POST, etc.) to make.
 *      retryOnAuthError (bool):
 *              If true, this function will attempt to refresh credentials and
 *              retry when there is an authentication error. If false, treat
 *              auth error as a failure.
 *      url (const char*):
 *              The base URL, not including query parameters.
 *      pQuery (Gdrive_Query*):
 *              Query parameters or HTTP POST data. The parameters will be
 *              either appended to the url or included in the HTTP request as 
 *              POST data, as appropriate for the requestType.
 *      pHeaders (struct curl_slist*):
 *              Any special HTTP headers to include in the request. The normal
 *              headers needed for any Google Drive requests will automatically
 *              be included, even if they are not in pHeaders. This argument can
 *              (and usually should) be NULL. After passing in a non-NULL 
 *              pHeaders, the calling function should NOT make any further use 
 *              of the passed curl_slist struct,and should NOT call 
 *              curl_slist_free_all().
 *      destFile (FILE*):
 *              A stream handle (normally for a temporary on-disk file) to store
 *              the response. If this argument is NULL, the response will be
 *              held in an in-memory buffer.
 * Return value (Gdrive_Download_Buffer*):
 *      A pointer to a Gdrive_Download_Buffer struct holding the response data 
 *      (or the FILE* handle to the stream to which the data was saved).
 */
Gdrive_Download_Buffer* gdrive_do_transfer(
        enum Gdrive_Request_Type requestType, bool retryOnAuthError, 
        const char* url,  const Gdrive_Query* pQuery, 
        struct curl_slist* pHeaders, FILE* destFile
);

/*
 * Should probably be moved into gdrive-transfer.c and made static. Not ready
 * for that yet.
 */
struct curl_slist* gdrive_get_authbearer_header(struct curl_slist* pHeaders);






#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_INTERNAL_H */

