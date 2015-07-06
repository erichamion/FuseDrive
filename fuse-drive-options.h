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
    
#define FUDR_DEFAULT_GDRIVE_ACCESS GDRIVE_ACCESS_WRITE
#define FUDR_DEFAULT_AUTH_BASENAME ".auth"
#define FUDR_DEFAULT_AUTH_RELPATH "fuse-drive"
#define FUDR_DEFAULT_CACHETTL 30
#define FUDR_DEFAULT_INTERACTION GDRIVE_INTERACTION_STARTUP
#define FUDR_DEFAULT_CHUNKSIZE GDRIVE_BASE_CHUNK_SIZE * 4
#define FUDR_DEFAULT_MAX_CHUNKS 15
#define FUDR_DEFAULT_FILE_PERMS 0644
#define FUDR_DEFAULT_DIR_PERMS 07777
    


typedef struct Fudr_Options
{
    int gdrive_access;
    char* gdrive_auth_file;
    time_t gdrive_cachettl;
    enum Gdrive_Interaction gdrive_interaction_type;
    size_t gdrive_chunk_size;
    int gdrive_max_chunks;
    unsigned long file_perms;
    unsigned long dir_perms;
    char** fuse_argv;
    int fuse_argc;
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

