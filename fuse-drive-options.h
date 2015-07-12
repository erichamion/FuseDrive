/* 
 * File:   fuse-drive-options.h
 * Author: me
 *
 * A struct and related functions for interpreting command line options and
 * for determining which options should be passed on to fuse_main().
 */

#ifndef FUSE_DRIVE_OPTIONS_H
#define	FUSE_DRIVE_OPTIONS_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <sys/types.h>
    
#include "gdrive/gdrive.h"
    
    

typedef struct Fudr_Options
{
    // Access level for Google Drive, one of the GDRIVE_ACCESS_* constants
    int gdrive_access;
    
    // Path to config/auth file
    char* gdrive_auth_file;
    
    // Time (in seconds) to assume cached data is still valid
    time_t gdrive_cachettl;
    
    // Determines when user interaction is allowed if Google Drive
    // authentication fails
    enum Gdrive_Interaction gdrive_interaction_type;
    
    // Size of file chunks
    size_t gdrive_chunk_size;
    
    // Maximum number of chunks per file
    int gdrive_max_chunks;
    
    // Permissions for files. Interpreted as a 3-digit octal number
    unsigned long file_perms;
    
    // Permissions for files. Interpreted as a 3-digit octal number
    unsigned long dir_perms;
    
    // Arguments to be passed on to FUSE
    char** fuse_argv;
    
    // Length of fuse_argv array
    int fuse_argc;
    
    // True if there was an error parsing command line options
    bool error;
    
    // If non-NULL, an error message that may be displayed to the user
    char* errorMsg;
} Fudr_Options;

/*
 * fudr_options_create():   Create a Fudr_Options struct and fill it with
 *                          values based on user-specified command line 
 *                          arguments.
 * Parameters:
 *      argc (int):     The same argc that was passed in to main().
 *      argv (char**):  The same argv that was passed in to main().
 * Return value (Fudr_Options*):
 *      Pointer to a Fudr_Options struct that has values based on the arguments
 *      in argv. Any options not specified have defined default values. This
 *      struct contains some pointer members, which should not be changed. When
 *      there is no more need for the struct, the caller is responsible for
 *      disposing of it with fudr_options_free().
 */
Fudr_Options* fudr_options_create(int argc, char** argv);

/*
 * fudr_options_free(): Safely free a Fudr_Options struct and any memory
 *                      associated with it.
 * Parameters:
 *      pOptions (Fudr_Options*):   A pointer previously returned by
 *                                  fudr_options_create(). The pointed-to memory
 *                                  will be freed and should no longer be used
 *                                  after this function returns.
 */
void fudr_options_free(Fudr_Options* pOptions);

#ifdef	__cplusplus
}
#endif

#endif	/* FUSE_DRIVE_OPTIONS_H */

