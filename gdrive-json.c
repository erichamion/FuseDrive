/* 
 * File:   gdrive-json.c
 * Author: me
 * 
 * A thin wrapper around the json-c library to reduce worries about reference
 * counts.  The caller typically only tracks and releases the root object, and 
 * most functions return copies of any needed values.  Also, anywhere a key's 
 * value is retrieved, the key can reflect nested objects in the form of 
 * "outer-key/inner-key1/inner-key2".
 *
 * Created on April 15, 2015 1:10 AM
 */

#include <stdbool.h>
#include <string.h>

#include "gdrive-json.h"

/*
 * The returned object should NOT be freed with gdrive_json_kill(), unless
 * gdrive_json_keep() is called first.
 * Only the final key can have a value of type array.
 * If key is a NULL pointer or an empty string, returns the same pObj that was
 * passed in.
 */
gdrive_json_object* gdrive_json_get_nested_object(gdrive_json_object* pObj, 
                                                  const char* key
)
{
    if (key == NULL || key[0] == '\0')
    {
        // No key, just return the original object.
        return pObj;
    }
    
    // Just use a single string guaranteed to be at least as long as the 
    // longest key (because it's the length of all the keys put together).
    char* currentKey = malloc(strlen(key) + 1);
    
    
    int startIndex = 0;
    int endIndex = 0;
    gdrive_json_object* pLastObj = pObj;
    gdrive_json_object* pNextObj;
    
    while (key[endIndex] != '\0')
    {
        // Find the '/' dividing keys or the null terminating the entire
        // set of keys.  After the for loop executes, the current key consists
        // of the range starting with (and including) startIndex, up to (but 
        // not including) endIndex.
        for (
                endIndex = startIndex; 
                key[endIndex] != '\0' && key[endIndex] != '/';
                endIndex++
                );  // This for loop intentionally has no body.
        
        // Copy the current key into a buffer and make sure it's null 
        // terminated.
        memcpy(currentKey, key + startIndex, endIndex - startIndex);
        currentKey[endIndex - startIndex + 1] = '\0';
        
        if (!json_object_object_get_ex(pLastObj, currentKey, &pNextObj))
        {
            // If the key isn't found, return NULL (by setting pNextObj to NULL
            // before eventually returning pNextObj).
            pLastObj = pNextObj = NULL;
            break;
        }
        
        pLastObj = pNextObj;
        startIndex = endIndex + 1;
    }
    
    
    free(currentKey);
    return pNextObj;
}

/*
 * On success:
 *      result is a null-terminated string, return value is the length including
 *      the null terminator.
 * if pObj does not contain the given key, or if the key's value  is not a 
 * string:
 *      Does not convert to string.  result is the empty string "", return value
 *      is -1.
 * If more than maxlen characters are required to hold the result (including
 * null terminator):
 *      result will be a null-terminated string containing the first 
 *      (maxlen - 1) characters.  Return value is the negative number whose 
 *      absolute value equals the number of additional bytes required to hold
 *      the entire string.
 */
int gdrive_json_get_string(gdrive_json_object* pObj, 
                           const char* key, 
                           char* result, 
                           int maxlen
)
{
    gdrive_json_object* pInnerObj = (key == NULL || key[0] == '\0') ? 
                                    pObj : 
                                    gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL || !json_object_is_type(pInnerObj, json_type_string))
    {
        // Key not found, or value is not a string.
        result[0] = '\0';
        return 0;
    }
    
    const char* jsonStr = json_object_get_string(pInnerObj);
    int sourcelen = strlen(jsonStr) + 1;
    strncpy(result, jsonStr, maxlen - 1);
    result[maxlen - 1] = '\0';
    return maxlen >= sourcelen ? sourcelen : maxlen - sourcelen;
}

/*
 * Succeeds only for numeric types (int or double), other types will not be 
 * converted.  If key is not found or the value corresponding to key is 
 * non-numeric, returns 0.  The value pointed to by pSuccess indicates success
 * (true) or failure (false).
 */
int64_t gdrive_json_get_int64(gdrive_json_object* pObj, 
                           const char* key, 
                           bool* pSuccess
)
{
    gdrive_json_object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL)
    {
        // Key not found, signal failure.
        *pSuccess = false;
        return 0;
    }
    if (!(json_object_is_type(pInnerObj, json_type_int) || 
            json_object_is_type(pInnerObj, json_type_double)))
    {
        // Non-numeric type, signal failure.
        *pSuccess = false;
        return 0;
    }
    
    *pSuccess = true;
    return json_object_get_int64(pInnerObj);

}

/*
 * Succeeds only for numeric types (int or double), other types will not be 
 * converted.  If key is not found or the value corresponding to key is 
 * non-numeric, returns 0.  The value pointed to by pSuccess indicates success
 * (true) or failure (false).
 */
double gdrive_json_get_double(gdrive_json_object* pObj, 
                           const char* key, 
                           bool* pSuccess
)
{
    gdrive_json_object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL)
    {
        // Key not found, signal failure.
        *pSuccess = false;
        return 0;
    }
    if (!(json_object_is_type(pInnerObj, json_type_int) || 
            json_object_is_type(pInnerObj, json_type_double)))
    {
        // Non-numeric type, signal failure.
        *pSuccess = false;
        return 0;
    }
    
    *pSuccess = true;
    return json_object_get_double(pInnerObj);

}

/*
 * pSuccess will be true only for literal boolean type.  If key is not found or 
 * the value corresponding to key is  not boolean, sets the value pointed to by
 * pSuccess to false, otherwise sets it to true.  If key is not found, also 
 * returns false. 
 * Note: The return value WILL convert other types to boolean, but the value 
 * pointed to by pSuccess will be false.  A false return value with false
 * *pSuccess is ambiguous, as this condition occurs both when a non-boolean
 * value is converted to false, and also when the key is not found.
 * Type conversion is as follows (adapted from json-c documentation): integer 
 * and double objects will return FALSE if their value is zero or TRUE 
 * otherwise. If the object is a string it will return TRUE if it has a non zero
 * length. For any other object type TRUE will be returned if the object is not 
 * NULL.
 */
bool gdrive_json_get_boolean(gdrive_json_object* pObj, 
                           const char* key, 
                           bool* pSuccess
)
{
    gdrive_json_object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL)
    {
        // Key not found, signal failure.
        *pSuccess = false;
        return false;
    }
    *pSuccess = json_object_is_type(pInnerObj, json_type_boolean);
    if (!(json_object_is_type(pInnerObj, json_type_int) || 
            json_object_is_type(pInnerObj, json_type_double)))
    {
        // Non-numeric type, signal failure.
        *pSuccess = false;
        return 0;
    }
    
    *pSuccess = true;
    return json_object_get_double(pInnerObj);
}

int gdrive_json_array_length(gdrive_json_object* pObj, const char* key)
{
    gdrive_json_object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL || !json_object_is_type(pInnerObj, json_type_array))
    {
        // Key not found or not an array, signal failure.
        return -1;
    }
    
    return json_object_array_length(pInnerObj);
}

/*
 * The json-c documentation seems unclear.  I don't think the object returned
 * should be freed with gdrive_json_kill().
 */
gdrive_json_object* gdrive_json_array_get(gdrive_json_object* pObj, 
                                          const char* key, 
                                          int index
)
{
    gdrive_json_object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL || !json_object_is_type(pInnerObj, json_type_array))
    {
        // Key not found, or object is not an array.  Return NULL for error.
        return NULL;
    }
    return json_object_array_get_idx(pInnerObj, index);
}

/*
 * The object returned will need to be freed with gdrive_json_kill().
 */
gdrive_json_object* gdrive_json_from_str(const char* inStr)
{
    // Is it this simple?
    return json_tokener_parse(inStr);
}

void gdrive_json_kill(gdrive_json_object* pObj)
{
    json_object_put(pObj);
}

void gdrive_json_keep(gdrive_json_object* pObj)
{
    json_object_get(pObj);
}