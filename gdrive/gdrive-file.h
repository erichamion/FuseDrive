/* 
 * File:   gdrive-file.h
 * Author: me
 * 
 * Functions for working with handles to open Google Drive files.
 * 
 * This header is part of the public Gdrive interface, and functions that appear
 * here can be used anywhere.
 *
 * Created on May 4, 2015, 11:07 PM
 */

#ifndef GDRIVE_FILE_H
#define	GDRIVE_FILE_H

// #ifdef	__cplusplus
// extern "C" {
// #endif
    
#include "gdrive-fileinfo.h"
    
#include <stdbool.h>
    
typedef struct Gdrive_Cache_Node Gdrive_File;
    
/*
 * gdrive_file_open():  Opens a specified Google Drive file and returns a handle
 *                      that can be used for file operations.
 * Parameters:
 *      fileId (const char*):
 *              The Google Drive file ID of the file to open. This must be the
 *              ID of a non-folder file.
 *      flags (int):
 *              File access flags. See standard documentation for open(2). The
 *              same flags must be passed to gdrive_file_close() when closing
 *              the file.
 *      pError (int*):
 *              A pointer to a memory location that will hold the specific error
 *              value if an error occurs. The pointed-to value does not change
 *              if the function succeeds.
 * Return value (Gdrive_File*):
 *      A file handle that can be used for operations such as reading the file's
 *      contents, or NULL on error. This handle must be passed to 
 *      gdrive_file_close() when the file is no longer needed.
 * NOTE:
 *      This function can be called multiple times for the same file, as long
 *      as each call is eventually balanced by a call to gdrive_file_close()
 *      with the returned file handle and the same flags.
 */
Gdrive_File* gdrive_file_open(const char* fileId, int flags, int* pError);

/*
 * gdrive_file_close(): Closes an open file, releasing any unnecessary 
 *                      resources.
 * Parameters:
 *      pFile (Gdrive_File*):
 *              A file handle returned by a prior call to gdrive_file_open().
 *      flags (int):
 *              The same flags that were used when calling gdrive_file_open().
 */
void gdrive_file_close(Gdrive_File* pFile, int flags);

/*
 * gdrive_file_read():  Reads the contents of an open file and adds the contents
 *                      of the read section to the cache.
 * Parameters:
 *      pFile (Gdrive_File*):
 *              A file handle returned by a prior call to gdrive_file_open().
 *      buf (char*):
 *              The location of a memory buffer into which to read the data. The
 *              buffer must already be allocated with at least size bytes. It is
 *              safe to pass a NULL pointer.
 *      size (size_t):
 *              The number of bytes to read.
 *      offset (off_t):
 *              The offset (zero-based, in bytes) from the start of the file at 
 *              which to start reading. 
 * Return value (int):
 *      On success, the actual number of bytes read (which may be less than the
 *      size argument if the end of the file was reached). On error, returns
 *      -1 times an error that could be returned from ferror(3).
 * TODO:
 *      Change the return type to size_t, and add a parameter to hold a pointer
 *      to an error value.
 */
int gdrive_file_read(Gdrive_File* fh, char* buf, size_t size, off_t offset);

/*
 * gdrive_file_write(): Write to the cached contents of an open file.
 * Parameters:
 *      pFile (Gdrive_File*):
 *              A file handle returned by a prior call to gdrive_file_open().
 *      buf (char*):
 *              The location of a memory buffer from which to read the data.
 *      size (size_t):
 *              The number of bytes to write.
 *      offset (off_t):
 *              The offset (zero-based, in bytes) from the start of the file at 
 *              which to start writing. 
 * Return value (int):
 *      On success, the actual number of bytes written. On error, returns
 *      -1 times an error that could be returned from ferror(3).
 * Note:
 *      The modified file will be uploaded to Google Drive when the file is 
 *      closed or when gdrive_file_sync() is called.
 * TODO:
 *      Change the return type to size_t, and add a parameter to hold a pointer
 *      to an error value.
 */
int gdrive_file_write(Gdrive_File* fh, const char* buf, size_t size, 
                      off_t offset
);

/*
 * gdrive_file_truncate():  Change the size of an open file. If increasing the
 *                          size, the end of the file will be filled with null
 *                          bytes.
 * Parameters:
 *      pFile (Gdrive_File*):
 *              A file handle returned by a prior call to gdrive_file_open().
 *      size (off_t):
 *              The new size of the file.
  * Return value (int):
 *      0 on success, a negative error numer on failure.
 */
int gdrive_file_truncate(Gdrive_File* fh, off_t size);

/*
 * gdrive_file_new():   Create a new file.
 * Parameters:
 *      path (const char*):
 *              The path for the new file.
 *      createFolder (bool):
 *              If true, the file created will be a folder. If false, it will be
 *              a regular file.
 *      pError (int*):
 *              Pointer to a memory location to hold an integer error value. On
 *              success, this will be 0.
 * Return value (char*):
 *      A pointer to a null-terminated string containing the Google Drive file
 *      ID of the newly created file. The caller is responsible for freeing the
 *      pointed-to memory.
 */
char* gdrive_file_new(const char* path, bool createFolder, int* pError);

/*
 * gdrive_file_sync():  Sync a file with Google Drive. In particular, if the 
 *                      file has been written to, then the modified file is
 *                      uploaded to Google Drive.
 * Parameters:
 *      fh (Gdrive_File*):
 *              The file handle for an open file to sync.
 * Return value:
 *      0 on success, a negative error number on failure.
 */
int gdrive_file_sync(Gdrive_File* fh);

/*
 * gdrive_file_sync():  Sync a file's metadata (the information stored in a 
 *                      Gdrive_Fileinfo struct) with Google Drive.
 * Parameters:
 *      fh (Gdrive_File*):
 *              The file handle for an open file to sync.
 * Return value:
 *      0 on success, a negative error number on failure.
 */
int gdrive_file_sync_metadata(Gdrive_File* fh);

/*
 * gdrive_file_set_atime(): Set the access time for an open file.
 * Parameters:
 *      fh (Gdrive_File*):
 *              A handle to an open file.
 *      ts (const struct timespec*):
 *              The pointer to a timespec struct representing the desired access
 *              time.
 * Note:
 *      This function will happily set a future time, but Google Drive doesn't
 *      seem to support future access times.
 */
int gdrive_file_set_atime(Gdrive_File* fh, const struct timespec* ts);

/*
 * gdrive_file_set_mtime(): Set the modification time for an open file.
 * Parameters:
 *      fh (Gdrive_File*):
 *              A handle to an open file.
 *      ts (const struct timespec*):
 *              The pointer to a timespec struct representing the desired 
 *              modification time.
 */
int gdrive_file_set_mtime(Gdrive_File* fh, const struct timespec* ts);



/*
 * gdrive_file_get_info():  Retrieve the file information for an open file.
 * Parameters:
 *      fh (Gdrive_File*):
 *              A file handle returned by a prior call to gdrive_file_open().
 * Return value (Gdrive_Fileinfo*):
 *      The pointer to a Gdrive_Fileinfo struct holding information on the file.
 *      The pointed-to location should not be freed by the caller.
 */
Gdrive_Fileinfo* gdrive_file_get_info(Gdrive_File* fh);

/*
 * gdrive_file_get_perms(): Retrieve the effective file permissions of an open
 *                          file.
 * Parameters:
 *      fh (Gdrive_File*):
 *              A file handle returned by a prior call to gdrive_file_open().
 * Return value (unsigned int):
 *      An integer value from 0 to 7 representing Unix filesystem permissions
 *      for the file. A permission needs to be present in both the Google Drive
 *      user's roles for the particular file and the overall access mode for the
 *      system. For example, if the Google Drive user has the owner role (both
 *      read and write access), but the system only has GDRIVE_ACCESS_READ, the
 *      returned value will be 4 (read access only).
 */
unsigned int gdrive_file_get_perms(const Gdrive_File* fh);


// #ifdef	__cplusplus
// }
// #endif

#endif	/* GDRIVE_FILE_H */

