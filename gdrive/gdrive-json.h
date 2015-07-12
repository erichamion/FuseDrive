/* 
 * File:   gdrive-json.h
 * Author: me
 * 
 * A thin wrapper around the json-c library to reduce worries about reference
 * counts.  The caller typically only tracks and releases the root object, and 
 * most functions return copies of any needed values.  Also, anywhere a key's 
 * value is retrieved, the key can reflect nested objects in the form of 
 * "outer-key/inner-key1/inner-key2".
 * 
 * This header is used internally by Gdrive code and should not be included 
 * outside of Gdrive code.
 *
 * Created on April 15, 2015, 1:20 AM
 */

#ifndef GDRIVE_JSON_H
#define	GDRIVE_JSON_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <json-c/json.h>
#include <stdbool.h>
    
typedef json_object Gdrive_Json_Object;

/*
 * gdrive_json_get_nested_object(): Retrieves a contained JSON object from 
 *                                  within the outer object.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The outer JSON object.
 *      key (const char*):
 *              The key whose object to retrieve. Can be several keys linked
 *              together with '/' to describe a key nested several layers deep,
 *              as long as none of the keys' values are arrays except 
 *              (optionally) the final one. For example, the key "foo/bar/baz"
 *              indicates that the original object holds the key "foo", foo's
 *              object holds the key "bar", bar's object holds the key "baz",
 *              and the function should return the object associated with baz.
 *              This example will fail if either foo's object or bar's object
 *              is an array.
 * Return value (Gdrive_Json_Object*):
 *      On success, the specified inner JSON object. On failure, NULL. The 
 *      returned object should NOT be freed with gdrive_json_kill().
 */
Gdrive_Json_Object* gdrive_json_get_nested_object(Gdrive_Json_Object* pObj, 
                                                  const char* key);

/*
 * gdrive_json_get_string():    Retrieves the value of a JSON string, copying
 *                              the string to a caller-allocated memory buffer.
 *                              Does not convert from non-string types to 
 *                              string.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The JSON object containing (directly or indirectly) the string,
 *              or if key is NULL, the actual JSON string object.
 *      key (const char*):
 *              If NULL, then pObj should be the actual JSON string object.
 *              Otherwise, the key whose value should be retrieved as a string.
 *              Can contain multiple nested keys, see 
 *              gdrive_json_get_nested_object().
 *      result (char*):
 *              The memory location at which to store the string. The caller is
 *              responsible for allocating at least maxlen bytes, as well as for
 *              freeing the memory when no longer needed. Can be NULL, which is
 *              useful for determining the memory needed for a given string and
 *              dynamically allocating it. If result is non-NULL and maxlen is
 *              greater than zero, then result will ALWAYS be null-terminated 
 *              after this function returns, even if there is an error.
 *      maxlen (long):
 *              The maximum length, in bytes, that can be stored in result, 
 *              including the null terminator. Must be non-negative, but can be
 *              zero. If the string that should be retrieved (not including the 
 *              terminating null) is longer than (maxlen - 1) bytes, then it is 
 *              truncated, and the truncated string is null-terminated.
 * Return value (long):
 *      If the string is successfully retrieved and copied to result, returns
 *      the length of the string, including the null terminator. If the string
 *      could not fit within maxlen bytes, returns (maxlen - actual length), 
 *      which is the negative number whose absolute value indicates how many
 *      additional bytes would be needed to store the whole string. On error
 *      (either pObj does not contain the given key, or the key's value is a
 *      numeric or other non-string type), returns INT64_MIN.
 * NOTE:
 *      Here is a summary of the possible conditions after this function 
 *      returns, assuming result is non-NULL.
 *      1. Testing:
 *              The caller passed in 0 for maxlen. The value stored at result is
 *              unchanged. The return value can be multiplied by -1 to get the
 *              number of bytes needed for this string.
 *      2. Success:
 *              The memory at result holds a null-terminated string. The return 
 *              value is the length of the string including the null terminator.
 *      3. Partial success:
 *              The memory at result holds a null-terminated string, which is
 *              only part of the actual string that we attempted to retrieve.
 *              The return value is negative and indicates how many bytes we
 *              were short.
 *      3. Failure:
 *              Either pObj did not contain the given key, or the key's value 
 *              was not a string. Does not convert to string. The memory at 
 *              result holds the empty string "", and the return value is 
 *              INT64_MIN.
 */
int64_t gdrive_json_get_string(Gdrive_Json_Object* pObj, const char* key, 
                            char* result, long maxlen);

/*
 * gdrive_json_get_new_string():    Retrieves the value of a JSON string, 
 *                                  copying the string to a newly allocated 
 *                                  memory buffer. Does not convert from 
 *                                  non-string types to string.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The JSON object containing (directly or indirectly) the string,
 *              or if key is NULL, the actual JSON string object.
 *      key (const char*):
 *              If NULL, then pObj should be the actual JSON string object.
 *              Otherwise, the key whose value should be retrieved as a string.
 *              Can contain multiple nested keys, see 
 *              gdrive_json_get_nested_object().
 *      pLength (long*):
 *              Can be NULL. If non-NULL, then after this function returns, 
 *              holds the size of the returned string in bytes, including the
 *              terminating null.
 * Return value (char*):
 *      On success, a newly allocated null-terminated string. On failure, NULL.
 *      The caller is responsible for freeing the returned string.
 * NOTE:
 *      If a caller wants to distinguish between memory allocation errors and
 *      other errors (key not found, value not a string), pLength can be used
 *      for this. On memory error, the value pointed to by pLength will be set
 *      to the intended length. On other errors, the value remains unchanged.
 */
char* gdrive_json_get_new_string(Gdrive_Json_Object* pObj, const char* key, 
                                 long* pLength);

/*
 * gdrive_json_realloc_string():    Retrieves the value of a JSON string, 
 *                                  copying the string to a memory buffer. Tries
 *                                  to use an existing buffer, or reallocates
 *                                  a new one if more space is needed. Does not 
 *                                  convert from non-string types to string.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The JSON object containing (directly or indirectly) the string,
 *              or if key is NULL, the actual JSON string object.
 *      key (const char*):
 *              If NULL, then pObj should be the actual JSON string object.
 *              Otherwise, the key whose value should be retrieved as a string.
 *              Can contain multiple nested keys, see 
 *              gdrive_json_get_nested_object().
 *      pDest (char**):
 *              The address of a pointer to an allocated memory buffer to hold
 *              the result string. The buffer must be one that can be realloc'ed
 *              if necessary. If the pointer at the given address changes as a
 *              result of calling this function, then any copies of the old
 *              pointer should no longer be used. The caller is responsible for
 *              freeing the memory pointed to by this pointer.
 *      pLength (long*):
 *              Must not be NULL. On input, points to the current length of the 
 *              buffer specified by pDest. Upon function return, points to the 
 *              size of the retrieved string in bytes, including the terminating
 *              null.
 * Return value (int):
 *      0 on success, other on failure.
 */
int gdrive_json_realloc_string(Gdrive_Json_Object* pObj, const char* key, 
                               char** pDest, long* pLength);

/*
 * gdrive_json_get_int64(): Retrieves an integer value from a JSON object.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The JSON object containing (directly or indirectly) the integer,
 *              or if key is NULL, the actual JSON string object.
 *      key (const char*):
 *              If NULL, then pObj should be the actual JSON integer object.
 *              Otherwise, the key whose value should be retrieved as an 
 *              integer. Can contain multiple nested keys, see 
 *              gdrive_json_get_nested_object().
 *      convertTypes (bool):
 *              Indicates whether to convert from non-numeric JSON types into
 *              integer.
 *      pSuccess (bool*):
 *              Must be non-NULL. Upon function return, the pointed to value
 *              indicates whether the function succeeded or failed. Failure
 *              conditions include key not found, as well as non-numeric type
 *              (if convertTypes is false).
 * Return value (int64_t):
 *      The retrieved integer, or 0 on failure.
 */
int64_t gdrive_json_get_int64(Gdrive_Json_Object* pObj, const char* key, 
                              bool convertTypes, bool* pSuccess);

/*
 * gdrive_json_get_double():    Retrieves a double floating point value from a 
 *                              JSON object. Does not convert from non-numeric
 *                              types to double.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The JSON object containing (directly or indirectly) the double,
 *              or if key is NULL, the actual JSON string object.
 *      key (const char*):
 *              If NULL, then pObj should be the actual JSON double object.
 *              Otherwise, the key whose value should be retrieved as an 
 *              double. Can contain multiple nested keys, see 
 *              gdrive_json_get_nested_object().
 *      pSuccess (bool*):
 *              Must be non-NULL. Upon function return, the pointed to value
 *              indicates whether the function succeeded or failed. Failure
 *              conditions include key not found and non-numeric type.
 * Return value (double):
 *      The retrieved double, or 0 on failure.
 */
double gdrive_json_get_double(Gdrive_Json_Object* pObj, const char* key, 
                              bool* pSuccess);

/*
 * gdrive_json_get_boolean():   Retrieves a boolean value from a JSON object. 
 *                              The returned value DOES convert from other types
 *                              to boolean, but the success indicator will only
 *                              be true if no conversion was needed.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The JSON object containing (directly or indirectly) the boolean,
 *              or if key is NULL, the actual JSON string object.
 *      key (const char*):
 *              If NULL, then pObj should be the actual JSON boolean object.
 *              Otherwise, the key whose value should be retrieved as an 
 *              boolean. Can contain multiple nested keys, see 
 *              gdrive_json_get_nested_object().
 *      pSuccess (bool*):
 *              Must be non-NULL. Upon function return, the pointed to value
 *              indicates whether the function succeeded or failed. Failure
 *              conditions include key not found and non-boolean type.
 * Return value (bool):
 *      If the key is found, returns the retrieved boolean value, or the actual 
 *      value converted to a boolean. If the key is not found, returns false.
 * NOTE:
 *      The return value WILL convert other types to boolean, but the value 
 *      pointed to by pSuccess will be false.  A false return value with false
 *      *pSuccess is ambiguous, as this condition occurs both when a non-boolean
 *      value is converted to false, and also when the key is not found.
 *      Type conversion is as follows (adapted from json-c documentation): 
 *      integer and double objects will return FALSE if their value is zero or 
 *      TRUE otherwise. If the object is a string it will return TRUE if it has 
 *      a non zero length. For any other object type TRUE will be returned if 
 *      the object is not NULL.
 */
bool gdrive_json_get_boolean(Gdrive_Json_Object* pObj, const char* key, 
                             bool* pSuccess);

/*
 * gdrive_json_from_string():   Creates a JSON object from a string 
 *                              representation.
 * Parameters:
 *      inStr (const char*):
 *              A null-terminated string representation of a JSON object.
 * Return value (Gdrive_Json_Object*):
 *      A JSON object, or NULL on error. The caller is responsible for calling
 *      gdrive_json_kill() with the returned value.
 */
Gdrive_Json_Object* gdrive_json_from_string(const char* inStr);

/*
 * gdrive_json_new():   Create a new empty JSON object.
 * Return value (Gdrive_Json_Object*):
 *      A JSON object, or NULL on error. The caller is responsible for calling
 *      gdrive_json_kill() with the returned value.
 */
Gdrive_Json_Object* gdrive_json_new();

/*
 * gdrive_json_add_string():    Adds a key/value pair to a JSON object. The 
 *                              value must be a string.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The parent object to which to add the new key/value pair.
 *      key (const char*):
 *              The key to add. This must be a single key, not nested.
 *      str (const char*):
 *              The string value to add. The string is copied, so the caller can
 *              alter or free the argument's memory if desired.
 */
void gdrive_json_add_string(Gdrive_Json_Object* pObj, const char* key, 
                            const char* str);

/*
 * gdrive_json_add_int64(): Adds a key/value pair to a JSON object. The value 
 *                          must be an integer.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The parent object to which to add the new key/value pair.
 *      key (const char*):
 *              The key to add. This must be a single key, not nested.
 *      str (const char*):
 *              The integer value to add.
 */
void gdrive_json_add_int64(Gdrive_Json_Object* pObj, const char* key, 
                           int64_t value);

/*
 * gdrive_json_add_double():    Adds a key/value pair to a JSON object. The 
 *                              value must be a double.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The parent object to which to add the new key/value pair.
 *      key (const char*):
 *              The key to add. This must be a single key, not nested.
 *      str (const char*):
 *              The double value to add.
 */
void gdrive_json_add_double(Gdrive_Json_Object* pObj, const char* key, 
                            double value);

/*
 * gdrive_json_add_boolean():   Adds a key/value pair to a JSON object. The 
 *                              value must be a boolean.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The parent object to which to add the new key/value pair.
 *      key (const char*):
 *              The key to add. This must be a single key, not nested.
 *      str (const char*):
 *              The boolean value to add.
 */
void gdrive_json_add_boolean(Gdrive_Json_Object* pObj, const char* key, 
                             bool value);

/*
 * gdrive_json_add_new_array(): Adds a key/value pair to a JSON object. The
 *                              value is a newly created empty JSON array.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The parent object to which to add the new key/value pair.
 *      key (const char*):
 *              The key to add. This must be a single key, not nested.
 * Return value (Gdrive_Json_Object*):
 *      A JSON object representing the newly created array. This returned value
 *      should NOT be freed with gdrive_json_kill().
 */
Gdrive_Json_Object* gdrive_json_add_new_array(Gdrive_Json_Object* pObj, 
                                              const char* key);

/*
 * gdrive_json_add_existing_array():   Adds a key/value pair to a JSON object. 
 *                                      The value is an existing JSON array.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The parent object to which to add the new key/value pair.
 *      key (const char*):
 *              The key to add. This must be a single key, not nested.
 *      pArray (Gdrive_Json_Object*):
 *              The JSON array to add.
 */
void gdrive_json_add_existing_array(Gdrive_Json_Object* pObj, const char* key, 
                                    Gdrive_Json_Object* pArray);

/*
 * gdrive_json_to_string(): Converts a JSON object to a string representation.
 * Parameters:
 *      pObj (Gdrive_Json_Object):
 *              The JSON object to convert.
 *      pretty (bool):
 *              If true, adds line breaks and whitespace for presentation. If 
 *              false, the string is more compact.
 * Return value (const char*):
 *      A string representation of the JSON object. The memory pointed to by the
 *      return value will be freed when the root JSON object is freed with
 *      gdrive_json_kill(). This function's return value should not be used
 *      after the root object is freed.
 */
const char* gdrive_json_to_string(Gdrive_Json_Object* pObj, bool pretty);

/*
 * gdrive_json_to_new_string(): Converts a JSON object to a string 
 *                              representation. Like gdrive_json_to_string(),
 *                              but the returned string is a copy that will
 *                              persist after the root JSON object is destroyed.
 * Parameters:
 *      pObj (Gdrive_Json_Object):
 *              The JSON object to convert.
 *      pretty (bool):
 *              If true, adds line breaks and whitespace for presentation. If 
 *              false, the string is more compact.
 * Return value (const char*):
 *      A string representation of the JSON object. The caller is responsible 
 *      for freeing the pointed-to memory.
 */
char* gdrive_json_to_new_string(Gdrive_Json_Object* pObj, bool pretty);

/*
 * gdrive_json_array_length():  Returns the number of objects in a JSON array.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The JSON object containing (directly or indirectly) the array,
 *              or if key is NULL, the actual JSON string object.
 *      key (const char*):
 *              If NULL, then pObj should be the actual JSON array object.
 *              Otherwise, the key whose value should be retrieved as an 
 *              array. Can contain multiple nested keys, see 
 *              gdrive_json_get_nested_object().
 * Return value (int):
 *      On success, the number of objects in the array. If the key is not found
 *      or the specified object is not an array, returns -1.
 */
int gdrive_json_array_length(Gdrive_Json_Object* pObj, const char* key);

/*
 * gdrive_json_array_get():  Returns the the object at a specified index in a 
 *                          JSON array.
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The JSON object containing (directly or indirectly) the array,
 *              or if key is NULL, the actual JSON string object.
 *      key (const char*):
 *              If NULL, then pObj should be the actual JSON array object.
 *              Otherwise, the key whose value should be retrieved as an 
 *              array. Can contain multiple nested keys, see 
 *              gdrive_json_get_nested_object().
 *      index (int):
 *              The index of the item to retrieve.
 * Return value (Gdrive_Json_Object*):
 *      On success, the JSON object at the specified index in the array. On
 *      failure, NULL. The returned value should NOT be freed with 
 *      gdrive_json_kill().
 */
Gdrive_Json_Object* gdrive_json_array_get(Gdrive_Json_Object* pObj, 
                                          const char* key, int index);

/*
 * gdrive_json_array_append_object():   Appends the specified JSON object to the
 *                                      end of a JSON array.
 * Parameters:
 *      pArray (Gdrive_Json_Object*):
 *              The JSON object that represents (not contains) the array.
 *      pNewObj (Gdrive_Json_Object*):
 *              The JSON object to add to the end of the array. This should be
 *              a root object (for example, returned by gdrive_json_new()) or
 *              an object that has been passed to gdrive_json_keep(). 
 * Return value (Gdrive_Json_Object*):
 *      Same as the return value for json_object_array_add(), which is not 
 *      documented.
 */
int gdrive_json_array_append_object(Gdrive_Json_Object* pArray, 
                                    Gdrive_Json_Object* pNewObj);

/*
 * gdrive_json_array_append_string():   Appends the specified string to the
 *                                      end of a JSON array.
 * Parameters:
 *      pArray (Gdrive_Json_Object*):
 *              The JSON object that represents (not contains) the array.
 *      val (const char*):
 *              The string to add. An internal copy is made, so the caller can
 *              modify or free the original after this function returns. 
 * Return value (Gdrive_Json_Object*):
 *      Same as the return value for json_object_array_add(), which is not 
 *      documented.
 */
int gdrive_json_array_append_string(Gdrive_Json_Object* pArray, 
                                    const char* val);

/*
 * gdrive_json_array_append_bool(): Appends the specified boolean value to the
 *                                  end of a JSON array.
 * Parameters:
 *      pArray (Gdrive_Json_Object*):
 *              The JSON object that represents (not contains) the array.
 *      val (bool):
 *              The value to add. 
 * Return value (Gdrive_Json_Object*):
 *      Same as the return value for json_object_array_add(), which is not 
 *      documented.
 */
int gdrive_json_array_append_bool(Gdrive_Json_Object* pArray, bool val);

/*
 * gdrive_json_array_append_double():   Appends the specified double to the
 *                                      end of a JSON array.
 * Parameters:
 *      pArray (Gdrive_Json_Object*):
 *              The JSON object that represents (not contains) the array.
 *      val (double):
 *              The value to add. 
 * Return value (Gdrive_Json_Object*):
 *      Same as the return value for json_object_array_add(), which is not 
 *      documented.
 */
int gdrive_json_array_append_double(Gdrive_Json_Object* pArray, double val);

/*
 * gdrive_json_array_append_int64():    Appends the specified integer to the
 *                                      end of a JSON array.
 * Parameters:
 *      pArray (Gdrive_Json_Object*):
 *              The JSON object that represents (not contains) the array.
 *      val (int64_t):
 *              The value to add. 
 * Return value (Gdrive_Json_Object*):
 *      Same as the return value for json_object_array_add(), which is not 
 *      documented.
 */
int gdrive_json_array_append_int64(Gdrive_Json_Object* pArray, int64_t val);

/*
 * gdrive_json_kill():  Release resources used by a root JSON object and all the
 *                      objects that depend on it (i.e., objects obtained with
 *                      gdrive_json_get_nested_object() or 
 *                      gdrive_json_array_get()).
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The root object to release.
 */
void gdrive_json_kill(Gdrive_Json_Object* pObj);

/*
 * gdrive_json_keep():  Marks a JSON object to be kept even when its root object
 *                      is killed. The caller is responsible for calling
 *                      gdrive_json_kill() on this object (the same number of
 *                      times that gdrive_json_keep() was called).
 * Parameters:
 *      pObj (Gdrive_Json_Object*):
 *              The JSON object to keep.
 */
void gdrive_json_keep(Gdrive_Json_Object* pObj);


#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_JSON_H */

