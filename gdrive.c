/* 
 * File:   GDrive.c
 * Author: me
 *
 * Created on December 29, 2014, 8:34 AM
 */

#define _XOPEN_SOURCE 500

#include <curl/curl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//#include "fuse-drive.h"
#include "gdrive.h"
#include "gdrive-internal.h"
//#include "gdrive-json.h"

/*
 * gdrive_init():   Initializes the network connection, sets appropriate 
 *                  settings for the Google Drive session, and ensures the user 
 *                  has granted necessary access permissions for the Google 
 *                  Drive account.
 */
int gdrive_init(Gdrive_Info** ppGdriveInfo, 
                int access, 
                const char* authFilename, 
                bool isInteractive
)
{
    // Allocate memory first.  Otherwise, if memory allocation fails, we won't
    // be able to tell that curl_global_init() was already called.
    if (*ppGdriveInfo == NULL)
    {
        if ((*ppGdriveInfo = malloc(sizeof(Gdrive_Info))) == NULL)
        {
            // Memory allocation failed, return error.  For now, return -1.
            // Later, we may define different error conditions.
            return -1;
        }
        memset(*ppGdriveInfo, 0, sizeof(Gdrive_Info));
    }
    Gdrive_Info* pInfo = *ppGdriveInfo;
    if (pInfo->internalInfo == NULL)
    {
        if ((pInfo->internalInfo = malloc(sizeof(Gdrive_Info_Internal))) == NULL)
        {
            // Memory allocation failed, return error.  For now, return -1.
            // Later, we may define different error conditions.
            return -1;
        }
        memset(pInfo->internalInfo, 0, sizeof(Gdrive_Info_Internal));
    }
    
    // If necessary, initialize curl.
    if (!(pInfo->internalInfo->isCurlInitialized) && 
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
    pInfo->internalInfo->isCurlInitialized = true;
    return gdrive_init_nocurl(ppGdriveInfo, 
                              access, 
                              authFilename, 
                              isInteractive
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
                bool isInteractive
)
{
    // Allocate any necessary struct memory.
    if (*ppGdriveInfo == NULL)
    {
        if ((*ppGdriveInfo = malloc(sizeof(Gdrive_Info))) == NULL)
        {
            // Memory allocation failed, return error.  For now, return -1.
            // Later, we may define different error conditions.
            return -1;
        }
        memset(*ppGdriveInfo, 0, sizeof(Gdrive_Info));
    }
    Gdrive_Info* pInfo = *ppGdriveInfo;
    if (pInfo->internalInfo == NULL)
    {
        if ((pInfo->internalInfo = malloc(sizeof(Gdrive_Info_Internal))) == NULL)
        {
            // Memory allocation failed, return error.  For now, return -1.
            // Later, we may define different error conditions.
            return -1;
        }
        memset(pInfo->internalInfo, 0, sizeof(Gdrive_Info_Internal));
    }
    
    // Assume curl_global_init() has already been called somewhere.
    pInfo->internalInfo->isCurlInitialized = true;
    
    // If a filename was given, attempt to open the file and read its contents.
    if (authFilename != NULL)
    {
        // Make sure the file exists and is a regular file (or if it's a 
        // symlink, it points to a regular file).
        struct stat st;
        if ((lstat(authFilename, &st) == 0) && (st.st_mode & S_IFREG))
        {
            // Try to get the auth token and refresh token, but it's not an
            // error if we can't get them.
            FILE* inFile = fopen(authFilename, "r");
            if (inFile != NULL)
            {
                char* buffer = malloc(st.st_size + 1);
                if (buffer != NULL)
                {
                    int bytesRead = fread(buffer, 1, st.st_size, inFile);
                    buffer[bytesRead>=0 ? bytesRead : 0] = '\0';
                     
                }
                
            }
        }
        
        
    }
    
    
    return 0;
}

