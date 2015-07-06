/* 
 * File:   gdrive-util.h
 * Author: me
 * 
 * Utility functions that are needed by various parts of the Gdrive code but
 * don't really seem to be part of the Gdrive functionality.
 * 
 * This header is used internally by Gdrive code and should not be included 
 * outside of Gdrive code.
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
    
/*
 * Gdrive_Path is used to separate the basename and dirname parts of a path.
 */
typedef struct Gdrive_Path Gdrive_Path;

/*
 * gdrive_path_create():    Create a Gdrive_Path struct from the specified path.
 * Parameters:
 *      path (const char*): 
 *              A null-terminated string holding the desired path.
 * Return value (Gdrive_Path*):
 *      A pointer to a Gdrive_Path struct that can be used to extract the 
 *      basename and dirname from the path. When no longer needed, this struct
 *      should be passed to gdrive_path_free().
 */
Gdrive_Path* gdrive_path_create(const char* path);

/*
 * gdrive_path_get_dirname():   Retrieve the directory part of a path, as
 *                              determined by the dirname() system call.
 * Parameters:
 *      gpath (const Gdrive_Path*): 
 *              A pointer returned by an earlier call to gdrive_path_create().
 * Return value (const char*):
 *      A pointer to a null-terminated string containing the dirname. The 
 *      pointed-to memory should not be altered or freed, and it should not be
 *      accessed after the gpath parameter is passed to gdrive_path_free().
 */
const char* gdrive_path_get_dirname(const Gdrive_Path* gpath);

/*
 * gdrive_path_get_basename():  Retrieve the basename part of a path, as
 *                              determined by the basename() system call.
 * Parameters:
 *      gpath (const Gdrive_Path*): 
 *              A pointer returned by an earlier call to gdrive_path_create().
 * Return value (const char*):
 *      A pointer to a null-terminated string containing the basename. The 
 *      pointed-to memory should not be altered or freed, and it should not be
 *      accessed after the gpath parameter is passed to gdrive_path_free().
 */
const char* gdrive_path_get_basename(const Gdrive_Path* gpath);

/*
 * gdrive_path_free():  Frees a Gdrive_Path struct and any associated memory.
 * Parameters:
 *      gpath (Gdrive_Path*):   
 *              The struct to free.
 */
void gdrive_path_free(Gdrive_Path* gpath);
   



/*
 * gdrive_divide_round_up():    Divide a dividend by a divisor. If there is a
 *                              remainder, round UP to the next integer.
 * Parameters:
 *      dividend (long):    
 *              The dividend.
 *      divisor (long):     
 *              The divisor.
 * Return value (long):
 *      The smallest integer greater than or equal to the result of 
 *      dividend / divisor.
 */
long gdrive_divide_round_up(long dividend, long divisor);



/*
 * gdrive_power_fopen():    Opens a file in a way similar to the fopen() system
 *                          call, but creates the parent directory (and the 
 *                          grandparent directory, and so on) if it doesn't
 *                          exist.
 * Parameters:
 *      path (const char*): 
 *              The path of the file to open.
 *      mode (const char*): 
 *              The mode parameter of the fopen() system call.
 * Return value:
 *      A valid FILE* handle on success, or NULL on failure.
 */
FILE* gdrive_power_fopen(const char* path, const char* mode);

/*
 * gdrive_recursive_mkdir():    Creates a directory in a way similar to the 
 *                              mkdir() system call, but creates the parent 
 *                              directory (and the grandparent directory, and so
 *                              on) if it doesn't exist.
 * Parameters:
 *      path (const char*): 
 *              The path of the directory to create.
 * Return value:
 *      Return value (int):
 *      0 on success. On error, returns a negative value whose absolute value
 *      is defined in <errors.h>
 */
int gdrive_recursive_mkdir(const char* path);





#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_UTIL_H */

