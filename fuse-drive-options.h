/* 
 * File:   fuse-drive-options.h
 * Author: me
 *
 * Created on July 5, 2015, 5:29 PM
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


Fudr_Options* fudr_options_create(int argc, char** argv);

void fudr_options_free(Fudr_Options* pOptions);

#ifdef	__cplusplus
}
#endif

#endif	/* FUSE_DRIVE_OPTIONS_H */

