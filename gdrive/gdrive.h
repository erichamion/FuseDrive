/* 
 * File:   gdrive.h
 * Author: me
 * 
 * Definitions and declarations for source files that use GDrive.c 
 * functionality. This is the main header file that should be included. This
 * header file and the files that are directly included by it (currently 
 * gdrive-fileinfo.h, gdrive-fileinfo-array.h, gdrive-sysinfo.h, and 
 * gdrive-file.h) form the public interface.
 * 
 *
 * Created on April 14, 2015, 9:29 PM
 */

#ifndef GDRIVE_H
#define	GDRIVE_H

#ifdef	__cplusplus
extern "C" {
#endif
    
    


/*
 * Access levels for Google Drive files.
 * GDRIVE_ACCESS_META:  File metadata only. Will enable directory listing, but
 *                      cannot open files.
 * GDRIVE_ACCESS_READ:  Read-only access to files. Implies GDRIVE_ACCESS_META.
 * GDRIVE_ACCESS_WRITE: Full read-write access to files. Implies
 *                      GDRIVE_ACCESS_READ and GDRIVE_ACCESS_META.
 * GDRIVE_ACCESS_APPS:  Read-only access to list of installed Google Drive apps.
 * GDRIVE_ACCESS_ALL:   Convenience value, includes all of the above.
 */
#define GDRIVE_ACCESS_META 0x01
#define GDRIVE_ACCESS_READ 0x02
#define GDRIVE_ACCESS_WRITE 0x04
#define GDRIVE_ACCESS_APPS 0x08
#define GDRIVE_ACCESS_ALL 0x0F
    
#define GDRIVE_BASE_CHUNK_SIZE 262144L

    
enum Gdrive_Interaction
{
    GDRIVE_INTERACTION_NEVER,
    GDRIVE_INTERACTION_STARTUP,
    GDRIVE_INTERACTION_ALWAYS
};

enum Gdrive_Filetype
{
    // May add a Google Docs file type or others
    GDRIVE_FILETYPE_FILE,
    GDRIVE_FILETYPE_FOLDER
};

typedef struct Gdrive_Info Gdrive_Info;



#include "gdrive-fileinfo.h"
#include "gdrive-fileinfo-array.h"
#include "gdrive-sysinfo.h"
#include "gdrive-file.h"



/******************
 * Fully public constructors, factory methods, destructors, and similar
 ******************/

/*
 * gdrive_init():   Initializes the network connection, sets appropriate 
 *                  settings for the Google Drive session, and ensures the user 
 *                  has granted necessary access permissions for the Google 
 *                  Drive account.  This function MUST be called  EXACTLY ONCE, 
 *                  at the start of the program, prior to any other gdrive_*() 
 *                  calls.  If multi-threaded mode is ever supported (not 
 *                  currently planned), this function must be called BEFORE any 
 *                  extra threads are created.
 *                  Note if using the curl library elsewhere: This function 
 *                  calls curl_global_init().
 * Parameters:
 *      access (int):
 *              One or more GDRIVE_ACCESS_* values (as defined in gdrive.h) 
 *              combined with bitwise OR.
 *      authFileName (char*):
 *              Optional filename for an authorization token file, or NULL
 *              to leave unspecified.  Does not need to be an existing file.  
 *              Existing auth token and refresh token will be read from this 
 *              file if it exists.  Any updated tokens will be written to the
 *              file.
 *      interactionMode (enum Gdrive_Interaction):   
 *              If the user has not already granted the requested permissions
 *              or has revoked permission, determines whether we can guide the 
 *              user through the authorization flow.  If not, treat insufficient
 *              permissions as an error.  Possible values are:
 *                  GDRIVE_INTERACTION_NEVER: No interaction with the user.  
 *                      Must have a previously saved access token and refresh 
 *                      token to avoid an error.
 *                  GDRIVE_INTERACTION_STARTUP: Prompt the user for 
 *                      authentication, if needed, during the gdrive_init() 
 *                      call only.  If permission is later revoked, do not
 *                      prompt at that point.
 *                  GDRIVE_INTERACTION_ALWAYS: Prompt the user for 
 *                      authentication any time it is needed.
 * Returns: 0 on success, other value on error.
 */
int gdrive_init(int access, 
                const char* authFilename, 
                time_t cacheTTL,
                enum Gdrive_Interaction interactionMode,
                size_t minFileChunkSize,
                int maxChunksPerFile
);

/*
 * gdrive_init_nocurl():    Sets appropriate settings for the Google Drive 
 *                          session and ensures the user has granted necessary 
 *                          access permissions for the Google Drive account.  
 *                          Similar to gdrive_init(), but does NOT initialize
 *                          the network connection (does not call 
 *                          curl_global_init()).  This should only be used if
 *                          the curl library is used elsewhere and 
 *                          curl_global_init() has already been called.
 */
int gdrive_init_nocurl(int access, 
                       const char* authFilename, 
                       time_t cacheTTL,
                       enum Gdrive_Interaction interactionMode,
                       size_t minFileChunkSize,
                       int maxChunksPerFile
);

/*
 * gdrive_cleanup():    Closes the network connection and cleanly frees the 
 *                      memory associated with the Google Drive session.  This 
 *                      function MUST be called EXACTLY ONCE, at the end of the 
 *                      program, after any other gdrive_*() calls.  If 
 *                      multi-threaded mode is ever supported (not currently 
 *                      planned), this function must be called AFTER any extra 
 *                      threads are finished.
 *                      Note if using the curl library elsewhere: This function 
 *                      calls curl_global_cleanup().
 */
void gdrive_cleanup(void);

/*
 * gdrive_cleanup_nocurl(): Cleanly frees the memory associated with the Google 
 *                          Drive session.  Similar to gdrive_cleanup(), but 
 *                          does NOT close the network connection (does not call 
 *                          curl_global_cleanup()).  This should only be used if
 *                          the curl library is used elsewhere and 
 *                          curl_global_cleanup() will be called elsewhere.
 */
void gdrive_cleanup_nocurl(void);


/******************
 * Fully public getter and setter functions
 ******************/

/*
 * gdrive_get_minchunksize():   Retrieves the file chunk size. Since files are 
 *                              stored in chunks of this size (or a multiple of 
 *                              this size), file operations such as reads may be
 *                              more efficient when they do not cross chunk 
 *                              boundaries.
 * Return value (size_t):
 *      The size, in bytes, of a file chunk.
 */
size_t gdrive_get_minchunksize(void);

/*
 * gdrive_get_maxchunks():  Maximum number of chunks in a downloaded file. When 
 *                          this value is higher, less data needs to be 
 *                          downloaded per chunk for large files, so small reads
 *                          (especially the first time a particular section of a
 *                          file is read) will be faster. Each chunk maintains 
 *                          an open file on disk, so this value should not be 
 *                          too high.
 * Return value (int):
 *      The maximum number of chunks per file.
 */
int gdrive_get_maxchunks(void);

/*
 * gdrive_get_filesystem_perms():   Retrieve the overall filesystem permissions
 *                                  for a particular type of file (currently 
 *                                  only distinguishes between folders and 
 *                                  non-folders).
 * Parameters:
 *      type (enum Gdrive_Filetype):
 *              The type of the file.
 * Return value (int):
 *      An integer value from 0 to 7, inclusive, representing Unix filesystem
 *      permissions. All files of the given type are limited to (at most) the
 *      returned permissions.
 */
int gdrive_get_filesystem_perms(enum Gdrive_Filetype type);


/******************
 * Other fully public functions
 ******************/

/*
 * gdrive_folder_list():    Retrieves a list of files within the given folder.
 * Parameters:
 *      folderId (const char*):
 *              The Google Drive file ID of the parent folder.
 * Return value (Gdrive_Fileinfo_Array*):
 *      A list of Gdrive_Fileinfo structs, each containing information on one
 *      file within the parent folder. The parent folder is not included in the
 *      list.
 */
Gdrive_Fileinfo_Array*  gdrive_folder_list(const char* folderId);

/*
 * gdrive_filepath_to_id(): Find the Google Drive file ID corresponding to a
 *                          given filepath.
 * Parameters:
 *      path (const char*):
 *              The file path within the Google Drive system, starting with "/"
 *              for the Google Drive root folder.
 * Return value (char*):
 *      A null-terminated string containing the file ID for the given path. The
 *      caller is responsible for freeing the memory at the returned location.
 */
char* gdrive_filepath_to_id(const char* path);

/*
 * gdrive_remove_parent():  Remove one folder from a file's list of parents.
 * Parameters:
 *      fileId (const char*):   
 *              The file ID of the file.
 *      parentId (const char*): 
 *              The file ID of the parent which should be removed.
 * Return value (int):
 *      0 on success. On error, returns a negative value whose absolute value
 *      is defined in <errors.h>
 */
int gdrive_remove_parent(const char* fileId, const char* parentId);

/*
 * gdrive_delete(): Delete a file by moving it to the trash.
 * Parameters:
 *      fileId (const char*):   
 *              The file ID of the file to delete.
 *      parentId (const char*): 
 *              The file ID of the parent folder. This folder will be removed 
 *              from the cache in order to maintain consistent information.
 * Return value (int):
 *      0 on success. On error, returns a negative value whose absolute value
 *      is defined in <errors.h>
 */
int gdrive_delete(const char* fileId, const char* parentId);

/*
 * gdrive_add_parent(): Add a parent to a given file.
 * Parameters:
 *      fileId (const char*):   
 *              The file ID of the file.
 *      parentId (const char*): 
 *              The file ID of the parent folder to add.
 * Return value (int):
 *      0 on success. On error, returns a negative value whose absolute value
 *      is defined in <errors.h>
 */
int gdrive_add_parent(const char* fileId, const char* parentId);

/*
 * gdrive_change_basename():    Rename a file without changing its parent 
 *                              directory/directories.
 * Parameters:
 *      fileId (const char*):   
 *              The file ID of the file to rename.
 *      newName (const char*):  
 *              The basename to which to change the file's name.
 * Return value (int):
 *      0 on success. On error, returns a negative value whose absolute value
 *      is defined in <errors.h>
 */
int gdrive_change_basename(const char* fileId, const char* newName);









#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_H */

