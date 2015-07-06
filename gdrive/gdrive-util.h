/* 
 * File:   gdrive-util.h
 * Author: me
 * 
 * Utility functions that are needed by various parts of the Gdrive code but
 * don't really seem to be part of the Gdrive functionality.
 * 
 * TODO: This has already been reduced to a single function. Review the other
 * modules to see if anything else should be put here. If not, just put the 
 * remaining function back into gdrive-info.c.
 *
 * Created on May 9, 2015, 12:00 PM
 */

#ifndef GDRIVE_UTIL_H
#define	GDRIVE_UTIL_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
    
    
typedef struct Gdrive_Path Gdrive_Path;

Gdrive_Path* gdrive_path_create(const char* path);
const char* gdrive_path_get_dirname(const Gdrive_Path* gpath);
const char* gdrive_path_get_basename(const Gdrive_Path* gpath);
void gdrive_path_free(Gdrive_Path* gpath);
    

long _gdrive_divide_round_up(long dividend, long divisor);


FILE* gdrive_power_fopen(const char* path, const char* mode);
int gdrive_recursive_mkdir(const char* path);



// For debugging purposes
void dumpfile(FILE* fh, FILE* dest);

// For temporary debugging only. This will have memory leaks
char* display_epochtime(time_t epochTime);
char* display_timespec(const struct timespec* tm);
char* display_epochtime_local(time_t epochTime);
char* display_timespec_local(const struct timespec* tm);



#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_UTIL_H */

