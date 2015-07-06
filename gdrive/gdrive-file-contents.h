/* 
 * File:   gdrive-file-contents.h
 * Author: me
 * 
 * A struct and related functions for managing a single portion, or chunk, of
 * a Google Drive file and saving the contents of the chunk to a temporary
 * on-disk file.
 * 
 * This header is used internally by Gdrive code and should not be included 
 * outside of Gdrive code.
 *
 * Created on May 6, 2015, 8:45 AM
 */

#ifndef GDRIVE_FILE_CONTENTS_H
#define	GDRIVE_FILE_CONTENTS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "gdrive-info.h"

    
typedef struct Gdrive_File_Contents Gdrive_File_Contents;

/*************************************************************************
 * Constructors, factory methods, destructors and similar
 *************************************************************************/

/*
 * gdrive_fcontents_add():  Creates a new Gdrive_File_Contents struct and any
 *                          required temporary files. Optionally adds the new
 *                          struct to an existing list of Gdrive_File_Contents
 *                          structs.
 * Parameters:
 *      pHead (Gdrive_File_Contents*):
 *              If non-NULL, a pointer to the head of an existing list to which
 *              to add the newly created struct.
 * Return value (Gdrive_File_Contents*):
 *      A pointer to the newly created struct. IF AND ONLY IF pHead was NULL,
 *      the caller is responsible for passing the address of this pointer (or 
 *      of a copy of the pointer) to any subsequent gdrive_fcontents_delete()
 *      calls, and also for passing the address of this pointer (or of a copy)
 *      to gdrive_fcontents_free_all().
 */
Gdrive_File_Contents* gdrive_fcontents_add(Gdrive_File_Contents* pHead);

/*
 * gdrive_fcontents_delete():   Removes a Gdrive_File_Contents struct from a
 *                              list of such structs, safely freeing its memory
 *                              and closing and deleting any associated 
 *                              temporary files.
 * Parameters:
 *      pContents (Gdrive_File_Contents*):
 *              A pointer to the struct to delete. The memory at the pointed-to
 *              location should no longer be used after this function returns.
 *      ppHead (Gdrive_File_Contents**):
 *              The address of a pointer to the head struct in the list. This
 *              pointer may be changed to point to a different memory location
 *              after this function returns. The new pointer should be passed to
 *              any subsequent gdrive_fcontents_delete() calls and eventually to
 *              gdrive_fcontents_free_all(). Any copies of the old pointer, if 
 *              different from the new one, should be discarded and should no
 *              longer be used or freed.
 */
void gdrive_fcontents_delete(Gdrive_File_Contents* pContents, 
                             Gdrive_File_Contents** ppHead
);

/*
 * gdrive_fcontents_delete_after_offset():  Safely removes and deletes select 
 *                                          Gdrive_File_Contents structs from
 *                                          a list of such structs. The structs
 *                                          selected for deletion are the ones
 *                                          whose starting offset is strictly
 *                                          greater than the given offset.
 * Parameters:
 *      ppHead (Gdrive_File_Contents**):
 *              The address of a pointer to the head struct in the list. This
 *              pointer may be changed to point to a different memory location
 *              after this function returns. The new pointer should be passed to
 *              any subsequent gdrive_fcontents_delete() calls and eventually to
 *              gdrive_fcontents_free_all(). Any copies of the old pointer, if 
 *              different from the new one, should be discarded and should no
 *              longer be used or freed.
 *      offset (off_t):
 *              Any Gdrive_File_Contents structs whose starting offset is 
 *              strictly greater than this argument will be deleted.
 */
void gdrive_fcontents_delete_after_offset(Gdrive_File_Contents** ppHead, 
                                          off_t offset
);

/*
 * gdrive_fcontents_free_all(): Safely frees the memory associated with an 
 *                              entire list of Gdrive_File_Contents structs and
 *                              closes and deletes any associated temporary 
 *                              files.
 * Parameters:
 *      ppContents (Gdrive_File_Contents**):
 *              The address of a pointer to the head struct in the list to be
 *              freed. The pointer at this memory location will be NULL after
 *              this function returns. If the caller has kept any copies of the
 *              old pointer, these should no longer be used.
 */
void gdrive_fcontents_free_all(Gdrive_File_Contents** ppContents);




/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

// No getters or setters


/*************************************************************************
 * Other accessible functions
 *************************************************************************/

/*
 * gdrive_fcontents_find_chunk():   Retrieves the Gdrive_File_Contents struct
 *                                  whose file chunk contains the specified
 *                                  offset from the start of the entire file,
 *                                  if it already exists.
 * Parameters:
 *      pHead (Gdrive_File_Contents*):
 *              A pointer to the head struct in the list to search.
 *      offset (off_t):
 *              The file offset to search for.
 * Return value (Gdrive_File_Contents*):
 *      A pointer to the Gdrive_File_Contents struct whose file chunk contains
 *      the specified offset, if such a Gdrive_File_Contents already exists.
 *      Otherwise, NULL.
 */
Gdrive_File_Contents* gdrive_fcontents_find_chunk(Gdrive_File_Contents* pHead,
                                                  off_t offset
);

/*
 * gdrive_fcontents_fill_chunk():   Download a chunk of a Google Drive file to
 *                                  temporary on-disk storage.
 * Parameters:
 *      pContents (Gdrive_File_Contents*):
 *              A pointer to the file contents struct that will describe the
 *              file chunk and will hold any references to on-desk temporary
 *              files.
 *      fileId (const char*):
 *              The Google Drive file ID of the file from which to download a
 *              chunk.
 *      start (off_t):
 *              The file offset (zero-based, inclusive, in bytes) at which to
 *              start the chunk within the entire Google Drive file.
 *      size (size_t):
 *              The number of bytes the chunk will hold. If start + size is 
 *              greater then the length of the file, only the actual file length
 *              will be stored.
 * Return value (int):
 *      0 on success, other on failure.
 */
int gdrive_fcontents_fill_chunk(Gdrive_File_Contents* pContents,
                                const char* fileId,
                                off_t start, 
                                size_t size
);

/*
 * gdrive_fcontents_read(): Reads from a file chunk's on-disk temporary file
 *                          into an in-memory buffer.
 * Parameters:
 *      pContents (Gdrive_File_Contents*):
 *              The Gdrive_File_Contents struct describing the file chunk from
 *              which to read.
 *      destBuf (char*):
 *              The in-memory buffer into which to read data. At least size 
 *              bytes should already be allocated at this location. It is safe
 *              to pass a NULL argument.
 *      offset (off_t):
 *              The offset (zero-based, in bytes) within the entire Google Drive
 *              file from which to start reading. Note: Unless this 
 *              Gdrive_File_Contents struct happens to hold the first chunk of
 *              the file (the chunk that starts at offset 0), this will NOT be
 *              the offset within the chunk.
 *      size (size_t):
 *              The number of bytes to read.
 * Return value (size_t):
 *      On success with a non-NULL destBuf, the number of bytes actually read. 
 *      If destBuf was NULL, returns the same value that would have been 
 *      returned on success. This will be less than size if either the entire 
 *      file or the chunk described by pContents ends. On error, the return 
 *      value is negative. The absolute value of the returned value will 
 *      correspond to the errors that can be returned by the ferror() system 
 *      call.
 */
size_t gdrive_fcontents_read(Gdrive_File_Contents* pContents, 
                             char* destBuf, 
                             off_t offset, 
                             size_t size
);

/*
 * gdrive_fcontents_write():    Write to a file chunk's on-disk temporary file
 *                              from an in-memory buffer.
 * Parameters:
 *      pContents (Gdrive_File_Contents*):
 *              The Gdrive_File_Contents struct describing the file chunk to
 *              which to write.
 *      buf (char*):
 *              The in-memory buffer from which to read data.
 *      offset (off_t):
 *              The offset (zero-based, in bytes) within the entire Google Drive
 *              file at which to start writing. Note: Unless this 
 *              Gdrive_File_Contents struct happens to hold the first chunk of
 *              the file (the chunk that starts at offset 0), this will NOT be
 *              the offset within the chunk.
 *      size (size_t):
 *              The number of bytes to write.
 *      extendChunk (bool):
 *              If true, the chunk will be extended if writing past the end. If
 *              false, writing will stop upon reaching the end of the chunk,
 *              even if fewer than size bytes have been written
 * Return value (off_t):
 *      On success the number of bytes actually written.  On error, the return 
 *      value is negative. The absolute value of the returned value will 
 *      correspond to the errors that can be returned by the ferror() system 
 *      call.
 */
off_t gdrive_fcontents_write(Gdrive_File_Contents* pContents, 
                             const char* buf, 
                             off_t offset,
                             size_t size,
                             bool extendChunk
);

/*
 * gdrive_fcontents_truncate(): Truncate a file chunk to a specified size.
 * Parameters:
 *      pContents (Gdrive_File_Contents*):
 *              The Gdrive_File_Contents struct describing the file chunk to
 *              truncate.
 *      size (size_t):
 *              The desired size of the chunk, in bytes.
 * Return value (int):
 *      0 on success, or a negative error number on failure.
 */
int gdrive_fcontents_truncate(Gdrive_File_Contents* pContents, size_t size);



#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_FILE_CONTENTS_H */

