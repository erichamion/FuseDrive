/* 
 * File:   gdrive-sysinfo.h
 * Author: me
 * 
 * A struct and related functions for managing overall filesystem information
 * such as used and free space.
 *
 * Created on May 8, 2015, 9:18 AM
 */

#ifndef GDRIVE_SYSINFO_H
#define	GDRIVE_SYSINFO_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "gdrive.h"
#include <sys/types.h>

typedef struct Gdrive_Sysinfo Gdrive_Sysinfo; 



/*************************************************************************
 * Constructors, factory methods, destructors and similar
 *************************************************************************/

// No constructors. This is a single struct instance that lives
// in static memory for the lifetime of the application. Members are retrieved
// using the gdrive_sysinfo_get_*() functions below.

/*
 * gdrive_sysinfo_cleanup():    Clears any dynamically allocated memory 
 *                              associated with the Gdrive_Sysinfo struct.
 */
void gdrive_sysinfo_cleanup();


/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

/*
 * gdrive_sysinfo_get_size():   Retrieves the total size of the user's Google
 *                              Drive quota.
 * Return value (int64_t):
 *      The total size, in bytes, of the user's Google Drive quota.
 */
int64_t gdrive_sysinfo_get_size(void);

/*
 * gdrive_sysinfo_get_used():   Retrieves the used portion of the user's Google
 *                              Drive quota.
 * Return value (int64_t):
 *      The bytes used out of of the user's Google Drive quota. The returned
 *      value includes storage used by any services that share a quota with
 *      Google Drive, not just by Google Drive files. This way, subtracting the
 *      used bytes from the total size will yield the actual amount of storage
 *      still available.
 */
int64_t gdrive_sysinfo_get_used(void);

/*
 * gdrive_sysinfo_get_rootid(): Retrieves the Google Drive file ID for the top
 *                              folder in the Google Drive filesystem.
 * Return value (const char*):
 *      A null-terminated string holding the file ID of the root Google Drive
 *      folder. The memory at the returned location should not be altered or
 *      freed.
 */
const char* gdrive_sysinfo_get_rootid(void);



/*************************************************************************
 * Other accessible functions
 *************************************************************************/

// No other public functions



#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_SYSINFO_H */

