/* 
 * File:   gdrive-fileinfo-array.h
 * Author: me
 * 
 * A struct and related functions to hold arrays of Gdrive_Fileinfo structs.
 * 
 * This header is part of the public Gdrive interface, and functions that appear
 * here can be used anywhere.
 *
 * Created on May 7, 2015, 9:04 PM
 */

#ifndef GDRIVE_FILEINFO_ARRAY_H
#define	GDRIVE_FILEINFO_ARRAY_H

// #ifdef	__cplusplus
// extern "C" {
// #endif
    
    
typedef struct Gdrive_Fileinfo_Array Gdrive_Fileinfo_Array;

#include "gdrive-fileinfo.h"
#include "gdrive-json.h"
    
 


/*************************************************************************
 * CConstructors, factory methods, destructors and similar
 *************************************************************************/

/*
 * gdrive_finfoarray_create():  Creates a new fileinfo array.
 * Parameters:
 *      maxSize (int):
 *              The maximum number of Gdrive_Fileinfo structs that can be stored
 *              in the new array.
 * Return value (Gdrive_Fileinfo_Array*):
 *      A pointer to a newly created fileinfo array that can hold up to maxSize
 *      Gdrive_Fileinfo structs. The caller is responsible for passing this
 *      return value to gdrive_finfoarray_free() once it is no longer needed.
 */
Gdrive_Fileinfo_Array* gdrive_finfoarray_create(int maxSize);

/*
 * gdrive_finfoarray_free():    Safely frees the memory associated with a
 *                              Gdrive_Fileinfo_Array struct.
 * Parameters:
 *      pArray (Gdrive_Fileinfo_Array*):
 *              A pointer to the array to free. This pointer should no longer be
 *              used after the function returns. Any Gdrive_Fileinfo pointers
 *              previously obtained from this array with 
 *              gdrive_finfoarray_get_*() should also no longer be used.
 */
void gdrive_finfoarray_free(Gdrive_Fileinfo_Array* pArray);


/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

/*
 * gdrive_finfoarray_get_first():   Retrieve the first item stored in a fileinfo
 *                                  array.
 * Parameters:
 *      pArray (Gdrive_Fileinfo_Array*):
 *              A pointer to the array.
 * Return value (const Gdrive_Fileinfo*):
 *      A pointer to the first item in the fileinfo array, or NULL if there are
 *      no items. NOTE: The memory pointed to by the return value will be freed
 *      when gdrive_finfoarray_free() is called.
 */
const Gdrive_Fileinfo* 
gdrive_finfoarray_get_first(Gdrive_Fileinfo_Array* pArray);

/*
 * gdrive_finfoarray_get_next():    Retrieve the next item stored in a fileinfo
 *                                  array.
 * Parameters:
 *      pArray (Gdrive_Fileinfo_Array*):
 *              A pointer to the array.
 *      pPrev (Gdrive_Fileinfo*):
 *              A pointer to the previous item, as obtained from a previous call
 *              to gdrive_finfoarray_get_first() or 
 *              gdrive_finfoarray_get_next().
 * Return value (const Gdrive_Fileinfo*):
 *      A pointer to the next item in the fileinfo array after pPrev, or NULL if
 *      no items remain. NOTE: The memory pointed to by the return value will be
 *      freed when gdrive_finfoarray_free() is called.
 */
const Gdrive_Fileinfo* 
gdrive_finfoarray_get_next(Gdrive_Fileinfo_Array* pArray, 
                               const Gdrive_Fileinfo* pPrev);

/*
 * gdrive_finfoarray_get_count():   Retrieves the number of items currently
 *                                  stored in a fileinfo array.
 * Parameters:
 *      pArray (Gdrive_Fileinfo_Array*):
 *              A pointer to the array.
 * Return value (int):
 *      The number of items stored in the array.
 */
int gdrive_finfoarray_get_count(Gdrive_Fileinfo_Array* pArray);


/*************************************************************************
 * Other accessible functions
 *************************************************************************/

/*
 * gdrive_finfoarray_add_from_json():   Adds a Gdrive_Fileinfo struct to a 
 *                                      fileinfo array, and fills in the details
 *                                      of the struct with information contained
 *                                      in a JSON object.
 * Parameters:
 *      pArray (Gdrive_Fileinfo_Array*):
 *              A pointer to the array.
 *      pObj (gdrive_json_object*):
 *              A pointer to the JSON object containing the file information.
 * Return value (int):
 *      0 on success, other on failure. Currently the only failure conditions
 *      are invalid arguments (one or more arguments is NULL) or exceeding the
 *      maximum number of stored fileinfo structs.
 */
int gdrive_finfoarray_add_from_json(Gdrive_Fileinfo_Array* pArray, 
                                        Gdrive_Json_Object* pObj);


// #ifdef	__cplusplus
// }
// #endif

#endif	/* GDRIVE_FILEINFO_ARRAY_H */

