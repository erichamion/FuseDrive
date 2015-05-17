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
 * Should probably be moved into gdrive-transfer.c and made static. Not ready
 * for that yet.
 */
struct curl_slist* gdrive_get_authbearer_header(struct curl_slist* pHeaders);






#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_INTERNAL_H */

