/* 
 * File:   gdrive-info.h
 * Author: me
 * 
 * Declarations and definitions to be used by various gdrive code and header
 * files, but which are not part of the public gdrive interface.
 * 
 * This header is used internally by Gdrive code and should not be included 
 * outside of Gdrive code.
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
#define GDRIVE_URL_UPLOAD "https://www.googleapis.com/upload/drive/v2/files"
#define GDRIVE_URL_ABOUT "https://www.googleapis.com/drive/v2/about"
#define GDRIVE_URL_CHANGES "https://www.googleapis.com/drive/v2/changes"
    

/******************
 * Semi-public constructors, factory methods, destructors and similar
 ******************/

/*
 * gdrive_get_info():   Retrieves the Gdrive_Info struct that contains settings
 *                      and general Google Drive state information.
 * Return value (Gdrive_Info*):
 *      A pointer to the Gdrive_Info struct. This is a pointer to a static 
 *      struct and should not be freed.
 */
Gdrive_Info* gdrive_get_info(void);


/******************
 * Semi-public getter and setter functions
 ******************/
    
/*
 * gdrive_get_curlhandle(): Retrieves a duplicate of the curl easy handle 
 *                          currently stored in the Gdrive_Info struct.
 * Return value (CURL*):
 *      A curl easy handle. The caller is responsible for calling 
 *      curl_easy_cleanup() on this handle.
 * NOTES:
 *      The Gdrive_Info struct stores a curl easy handle with several options
 *      pre-set. In order to maintain consistency and avoid corrupting the
 *      handle's options (as could happen if an option intended for one specific
 *      operation is not reset to the default after the operation finishes),
 *      this function returns a new copy of the handle, not a reference to the
 *      existing handle.
 */
CURL* gdrive_get_curlhandle(void);

/*
 * gdrive_get_access_token():   Retrieve the current access token.
 * Return value (const char*):
 *      A pointer to a null-terminated string, or a NULL pointer if there is no
 *      current access token. The pointed-to memory should not be altered or
 *      freed.
 */
const char* gdrive_get_access_token(void);


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
    


#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_INTERNAL_H */

