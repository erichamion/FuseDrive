/* 
 * File:   gdrive-cache.h
 * Author: me
 * 
 * This header file should be included by gdrive-internal.h
 * 
 * A struct and related functions for managing cached data. There are two 
 * in-memory caches. One is a mapping from file pathnames to Google Drive 
 * file IDs, and the other holds basic file information such as size and 
 * access time (along with information about any open files and their on-disk
 * cached contents).
 *
 * Created on May 3, 2015, 9:10 PM
 */

#ifndef GDRIVE_CACHE_H
#define	GDRIVE_CACHE_H

#ifdef	__cplusplus
extern "C" {
#endif
   
    
#include "gdrive.h"
#include "gdrive-fileid-cache-node.h"
#include "gdrive-cache-node.h"
    
    
typedef struct Gdrive_Cache Gdrive_Cache;

/*************************************************************************
 * Constructors and destructors
 *************************************************************************/

/*
* gdrive_cache_init():  Initializes the cache.
 * Parameters:
 *      cacheTTL (time_t):
 *              The time (in seconds) for which cached data is considered good.
 *              When an item is retrieved from the cache, if more than cacheTTL 
 *              seconds have passed since both the creation of the item being 
 *              retrieved and the last time the cache was updated, the cache 
 *              will be updated by getting a list of changes from Google Drive.
 * Return value (int):
 *      0 on success, other on failure.
 */
int gdrive_cache_init(time_t cacheTTL);

/*
 * gdrive_cache_get():  Retrieves a pointer to the cache.
 * Return value (const Gdrive_Cache*):
 *      A read-only pointer to the cache.
 */
const Gdrive_Cache* gdrive_cache_get(void);

/*
 * gdrive_cache_cleanup():  Safely frees memory and files associated with the 
 *                          cache.
 */
void gdrive_cache_cleanup(void);



/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

/*
 * gdrive_cache_get_fileidcachehead():  Retrieves the first item in the 
 *                                      file ID cache.
 * Return value (Gdrive_Fileid_Cache_Node*):
 *      A pointer to the first item in the file ID cache.
 */
Gdrive_Fileid_Cache_Node* gdrive_cache_get_fileidcachehead();

/*
 * gdrive_cache_get_ttl():  Returns the number of seconds for which cached data
 *                          is considered good.
 *      The time (in seconds) for which cached data is considered good and does
 *      not need refreshed.
 */
time_t gdrive_cache_get_ttl();

/*
 * gdrive_cache_get_ttl():  Retrieves the time (in seconds since the epoch) the
 *                          cache was last updated.
 * Return value (time_t):
 *      The time the cache was last updated.
 */
time_t gdrive_cache_get_lastupdatetime();

/*
 * gdrive_cache_get_nextchangeid(): Retrieves the next change ID value from the
 *                                  last time the cache was updated. This value
 *                                  can be used to determine whether changes 
 *                                  have been made since the last cache update.
 * Return value (int64_t):
 *      The next change ID (which is 1 higher than the "largestChangeId" from
 *      Google Drive).
 */
int64_t gdrive_cache_get_nextchangeid();




/*************************************************************************
 * Other accessible functions
 *************************************************************************/

/*
 * gdrive_cache_update_if_stale():  If the cache has not been updated within
 *                                  cacheTTL seconds, updates by getting a list
 *                                  of changes from Google Drive.
 * Return value (int):
 *      0 on success, other on error.
 */
int gdrive_cache_update_if_stale();

/*
 * gdrive_cache_update():   Updates the cache by getting a list of changes from 
 *                          Google Drive.
 * Return value (int):
 *      0 on success, other on error.
 */
int gdrive_cache_update();

Gdrive_Fileinfo* gdrive_cache_get_item(const char* fileId,
                                       bool addIfDoesntExist,
                                       bool* pAlreadyExists
);

/*
 * gdrive_cache_add_fileid():   Stores a (pathname -> file ID) mapping in the
 *                              file ID cache maintained by pCache, maintaining
 *                              the uniqueness of the pathname. The argument
 *                              strings are copied, so the caller can safely
 *                              free them if desired.
 * Parameters:
 *      path (const char*):
 *              The full pathname of a file or folder on Google Drive, expressed
 *              as an absolute path within the Google Drive filesystem. The root
 *              Drive folder is "/". A file named "FileX", whose parent is a 
 *              folder named "FolderA", where FolderA is directly inside the 
 *              root folder, would be specified as "/FolderA/FileX". NOTE: The
 *              paths cached are unique, as any one path will have only one file
 *              ID. If the path argument is identical (based on string 
 *              comparison) to a path already in the file ID cache, the existing
 *              item will be updated, replacing the existing file ID with a copy
 *              of the fileId argument.
 *      fileId (const char*):
 *              The Google Drive file ID of the specified file. Because Google
 *              Drive allows a single file (with a single file ID) to have
 *              multiple parents (each resulting in a different path), fileId
 *              does not need to be unique. The same file ID can correspond to
 *              multiple paths.
 * Return value (int):
 *      0 on success, other on failure.
 */
int gdrive_cache_add_fileid(const char* path, const char* fileId);

/*
 * gdrive_cache_get_node(): Retrieves a pointer to the cache node used to store
 *                          information about a file and to manage on-disk 
 *                          cached file contents. Optionally creates the node
 *                          if it doesn't already exist in the cache.
 * Parameters:
 *      fileId (const char*):
 *              The Google Drive file ID identifying the file whose node to
 *              retrieve.
 *      addIfDoesntExist (bool):
 *              If true and the file ID is not already in the cache, a new node
 *              will be created.
 *      pAlreadyExists (bool*):
 *              Can be NULL. If addIfDoesntExist is true, then the bool stored 
 *              at pAlreadyExists will be set to true if the file ID already 
 *              existed in the cache, or false if a new node was created. The
 *              value stored at this memory location is undefined if 
 *              addIfDoesntExist was false.
 * Return value (Gdrive_Cache_Node*):
 *      If the file ID given by the fileId parameter already exists in the 
 *      cache, returns a pointer to the cache node describing the specified 
 *      file. If the file ID is not in the cache, then returns a pointer to a
 *      newly created cache node with the file ID filled in if addIfDoesntExist
 *      was true, or NULL if addIfDoesntExist was false. The returned cache node
 *      pointer can also be used as a Gdrive_Filehandle* pointer.
 * TODO:    Change this to gdrive_cache_get_filehandle() and return a
 *          Gdrive_Filehandle*.
 */
Gdrive_Cache_Node* gdrive_cache_get_node(const char* fileId, 
                                         bool addIfDoesntExist, 
                                         bool* pAlreadyExists
);

/*
 * gdrive_cache_get_fileid():   Retrieve from the File ID cache the Google Drive
 *                              file ID corresponding to a given path.
 * Parameters:
 *      path (const char*):
 *              A string containing the pathname within the Google Drive 
 *              directory structure (the Google Drive root folder is "/").
 * Return value:
 *      On success, a char* null-terminated string holding the Google Drive file
 *      ID of the specified file. On failure, NULL. The memory location returned
 *      should not be altered or freed.
 */
const char* gdrive_cache_get_fileid(const char* path);

void gdrive_cache_delete_id(const char* fileId);

void gdrive_cache_delete_node(Gdrive_Cache_Node* pNode);





    

#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_CACHE_H */

