/* 
 * File:   gdrive-fileinfo.h
 * Author: me
 * 
 * A struct and related functions for working with basic file information.
 * 
 * This header is part of the public Gdrive interface, and functions that appear
 * here can be used anywhere.
 *
 * Created on May 5, 2015, 9:42 PM
 */

#ifndef GDRIVE_FILEINFO_H
#define	GDRIVE_FILEINFO_H

#ifdef	__cplusplus
extern "C" {
#endif
    
typedef struct Gdrive_Fileinfo Gdrive_Fileinfo;


#include <time.h>
#include "gdrive.h"
#include "gdrive-json.h"


#define GDRIVE_TIMESTRING_LENGTH 31

    
typedef struct Gdrive_Fileinfo
{
    char* id;   // The Google Drive file ID of the file
    char* filename; // The filename with extension (not the full path)
    enum Gdrive_Filetype type;  // The type of file
    size_t size;    // File size in bytes
    int basePermission; // File permission, does not consider the access mode.
    struct timespec creationTime;
    struct timespec modificationTime;
    struct timespec accessTime;
    int nParents;   // Number of parent directories
    int nChildren;  // Number of children if type is GDRIVE_FILETYPE_FOLDER
    bool dirtyMetainfo; // Currently only tracks accessTime and modificationTime
} Gdrive_Fileinfo;





/*************************************************************************
 * Constructors, factory methods, destructors and similar
 *************************************************************************/

/*
 * gdrive_finfo_get_by_id():    Retrieves a Gdrive_Fileinfo struct describing
 *                              the file corresponding to a given Google Drive
 *                              file ID.
 * Parameters:
 *      fileId (const char*):
 *              The Google Drive file ID of the file for which to get 
 *              information.
 * Return value (const Gdrive_Fileinfo*):
 *      A pointer to a Gdrive_Fileinfo struct describing the file. The memory
 *      at the pointed-to location should not be freed or altered.
 */
const Gdrive_Fileinfo* gdrive_finfo_get_by_id(const char* fileId);

/*
 * gdrive_finfo_cleanup():  Safely frees any memory pointed to by members of a
 *                          Gdrive_Fileinfo struct, then sets all the members to
 *                          0 or NULL. Does NOT free the struct itself.
 * Parameters:
 *      pFileinfo (Gdrive_Fileinfo*):
 *              A pointer to the Gdrive_Fileinfo struct to clear.
 */
void gdrive_finfo_cleanup(Gdrive_Fileinfo* pFileinfo);




/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

/*
 * gdrive_finfo_get_atime_string(): Retrieve a file's access time as a string
 *                                  in RFC3339 format.
 * Parameters:
 *      pFileinfo (Gdrive_Fileinfo*):
 *              The Gdrive_Fileinfo struct from which to retrieve the time.
 *      dest (char*):
 *              A destination buffer which will receive the time string. This
 *              should already be allocated with at least max bytes.
 *      max (size_t):
 *              The maximum number of bytes, including the terminating null, to
 *              place into dest.
 * Return value (int):
 *      The number of bytes that were placed in dest, excluding the terminating
 *      null.
 */
int gdrive_finfo_get_atime_string(Gdrive_Fileinfo* pFileinfo, 
                                  char* dest, 
                                  size_t max
);

/*
 * gdrive_finfo_set_atime():    Set the access time in a Gdrive_Fileinfo struct.
 * Parameters:
 *      pFileinfo (Gdrive_Fileinfo*):
 *              A pointer to the struct whose time should be set.
 *      ts (const struct timespec*):
 *              A pointer to a timespec struct representing the time.
 * Return value:
 *      0 on success, non-zero on failure.
 */
int gdrive_finfo_set_atime(Gdrive_Fileinfo* pFileinfo, 
                           const struct timespec* ts
);

/*
 * gdrive_finfo_get_atime_string(): Retrieve a file's creation time as a string
 *                                  in RFC3339 format.
 * Parameters:
 *      pFileinfo (Gdrive_Fileinfo*):
 *              The Gdrive_Fileinfo struct from which to retrieve the time.
 *      dest (char*):
 *              A destination buffer which will receive the time string. This
 *              should already be allocated with at least max bytes.
 *      max (size_t):
 *              The maximum number of bytes, including the terminating null, to
 *              place into dest.
 * Return value (int):
 *      The number of bytes that were placed in dest, excluding the terminating
 *      null.
 */
int gdrive_finfo_get_ctime_string(Gdrive_Fileinfo* pFileinfo, 
                                  char* dest, 
                                  size_t max
);

/*
 * gdrive_finfo_get_atime_string(): Retrieve a file's modification time as a 
 *                                  string in RFC3339 format.
 * Parameters:
 *      pFileinfo (Gdrive_Fileinfo*):
 *              The Gdrive_Fileinfo struct from which to retrieve the time.
 *      dest (char*):
 *              A destination buffer which will receive the time string. This
 *              should already be allocated with at least max bytes.
 *      max (size_t):
 *              The maximum number of bytes, including the terminating null, to
 *              place into dest.
 * Return value (int):
 *      The number of bytes that were placed in dest, excluding the terminating
 *      null.
 */
int gdrive_finfo_get_mtime_string(Gdrive_Fileinfo* pFileinfo, 
                                  char* dest, 
                                  size_t max
);

/*
 * gdrive_finfo_set_atime():    Set the modification time in a Gdrive_Fileinfo 
 *                              struct.
 * Parameters:
 *      pFileinfo (Gdrive_Fileinfo*):
 *              A pointer to the struct whose time should be set.
 *      ts (const struct timespec*):
 *              A pointer to a timespec struct representing the time.
 * Return value:
 *      0 on success, non-zero on failure.
 */
int gdrive_finfo_set_mtime(Gdrive_Fileinfo* pFileinfo, 
                           const struct timespec* ts
);



/*************************************************************************
 * Other accessible functions
 *************************************************************************/

/*
 * gdrive_finfo_read_json():    Fill the given Gdrive_Fileinfo struct with 
 *                              information contained in a JSON object.
 * Parameters:
 *      pFileinfo (Gdrive_Fileinfo*):
 *              A pointer to the fileinfo struct to fill.
 *      pObj (gdrive_json_object*):
 *              A JSON object representing a File resource on Google Drive.
 */
void gdrive_finfo_read_json(Gdrive_Fileinfo* pFileinfo, 
                            Gdrive_Json_Object* pObj
);

/*
 * gdrive_finfo_real_perms():   Retrieve the actual effective permissions for
 *                              the file described by a given Gdrive_Fileinfo
 *                              struct.
 * Parameters:
 *      pFileinfo (const Gdrive_Fileinfo*):
 *              Pointer to an existing Gdrive_Fileinfo struct that has at least 
 *              the basePermission and type members filled in.
 * Return value (unsigned int):
 *      An integer value from 0 to 7 representing Unix filesystem permissions
 *      for the file. A permission needs to be present in both the Google Drive
 *      user's roles for the particular file and the overall access mode for the
 *      system. For example, if the Google Drive user has the owner role (both
 *      read and write access), but the system only has GDRIVE_ACCESS_READ, the
 *      returned value will be 4 (read access only).
 */
unsigned int gdrive_finfo_real_perms(const Gdrive_Fileinfo* pFileinfo);




    

#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_FILEINFO_H */

