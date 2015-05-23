/* 
 * File:   gdrive-json.c
 * Author: me
 * 
 * 
 *
 * Created on April 15, 2015 1:10 AM
 */

#include "gdrive-json.h"

#include <string.h>


Gdrive_Json_Object* gdrive_json_get_nested_object(Gdrive_Json_Object* pObj, 
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
    Gdrive_Json_Object* pLastObj = pObj;
    Gdrive_Json_Object* pNextObj;
    
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
        currentKey[endIndex - startIndex] = '\0';
        
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


int64_t gdrive_json_get_string(Gdrive_Json_Object* pObj, 
                            const char* key, 
                            char* result, 
                            long maxlen
)
{
    if (maxlen < 0)
    {
        // Invalid argument
        return INT64_MIN;
    }
    
    Gdrive_Json_Object* pInnerObj = (key == NULL || key[0] == '\0') ? 
                                    pObj : 
                                    gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL || !json_object_is_type(pInnerObj, json_type_string))
    {

        // Key not found, or value is not a string.
        if (result != NULL)
        {
            result[0] = '\0';
        }
        return INT64_MIN;
    }
    
    const char* jsonStr = json_object_get_string(pInnerObj);
    int sourcelen = strlen(jsonStr) + 1;
    if (result != NULL && maxlen > 0)
    {
        strncpy(result, jsonStr, maxlen - 1);
        result[maxlen - 1] = '\0';
    }
    return maxlen >= sourcelen ? sourcelen : maxlen - sourcelen;
}

char* gdrive_json_get_new_string(Gdrive_Json_Object* pObj, 
                                 const char* key,
                                 long* pLength
)
{
    // Find the length of the string.
    long length = gdrive_json_get_string(pObj, key, NULL, 0);
    if (length == INT64_MIN || length == 0)
    {
        // Not a string (length includes the null terminator, so it can't
        // be 0).
        return NULL;
    }

    length *= -1;
    // Allocate enough space to store the retrieved string.
    char* result = malloc(length);
    if (pLength != NULL)
    {
        *pLength = length;
    }
    if (result == NULL)
    {
        // Memory allocation error
        if (pLength != NULL)
        {
            *pLength = 0;
        }
        return NULL;
    }

    // Actually retrieve the string.
    gdrive_json_get_string(pObj, key, result, length);
    return result;
}

int gdrive_json_realloc_string(Gdrive_Json_Object* pObj, 
                                     const char* key,
                                     char** pDest, 
                                     long* pLength
)
{
    // Find the length of the string.
    long length = gdrive_json_get_string(pObj, key, NULL, 0);
    if (length == INT64_MIN || length == 0)
    {
        // Not a string (length includes the null terminator, so it can't
        // be 0).
        return -1;
    }

    length *= -1;
    // Make sure we have enough space to store the retrieved string.
    // If not, make more space with realloc.
    if (*pLength < length)
    {
        *pDest = realloc(*pDest, length);
        *pLength = length;
        if (*pDest == NULL)
        {
            // Memory allocation error
            *pLength = 0;
            return -1;
        }
    }

    // Actually retrieve the string.  This should return a non-zero positive
    // number, so determine success or failure based on that condition.
    return (gdrive_json_get_string(pObj, key, *pDest, length) > 0) ?
        0 :
        -1;
}

int64_t gdrive_json_get_int64(Gdrive_Json_Object* pObj,
                              const char* key,
                              bool convertTypes, 
                              bool* pSuccess
)
{
    Gdrive_Json_Object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL)
    {
        // Key not found, signal failure.
        *pSuccess = false;
        return 0;
    }
    if (!convertTypes &&
            !(json_object_is_type(pInnerObj, json_type_int) || 
            json_object_is_type(pInnerObj, json_type_double))
            )
    {
        // Non-numeric type, signal failure.
        *pSuccess = false;
        return 0;
    }
    
    *pSuccess = true;
    return json_object_get_int64(pInnerObj);

}

double gdrive_json_get_double(Gdrive_Json_Object* pObj, 
                              const char* key, 
                              bool* pSuccess
)
{
    Gdrive_Json_Object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
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

bool gdrive_json_get_boolean(Gdrive_Json_Object* pObj, 
                             const char* key, 
                             bool* pSuccess
)
{
    Gdrive_Json_Object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
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

Gdrive_Json_Object* gdrive_json_from_string(const char* inStr)
{
    return json_tokener_parse(inStr);
}

Gdrive_Json_Object* gdrive_json_new()
{
    return json_object_new_object();
}

void gdrive_json_add_string(Gdrive_Json_Object* pObj, 
                            const char* key, 
                            const char* str
)
{
    // json_object_new_xxxx should give a reference count of 1, then
    // json_object_object_add should decrement the count.  Everything should
    // balance, and the caller shouldn't need to worry about reference counts
    // or ownership.
    json_object_object_add(pObj, key, json_object_new_string(str));
}

void gdrive_json_add_int64(Gdrive_Json_Object* pObj,
                           const char* key, 
                           int64_t value
)
{
    // json_object_new_xxxx should give a reference count of 1, then
    // json_object_object_add should decrement the count.  Everything should
    // balance, and the caller shouldn't need to worry about reference counts
    // or ownership.
    json_object_object_add(pObj, key, json_object_new_int64(value));
}

void gdrive_json_add_double(Gdrive_Json_Object* pObj, 
                            const char* key, 
                            double value
)
{
    // json_object_new_xxxx should give a reference count of 1, then
    // json_object_object_add should decrement the count.  Everything should
    // balance, and the caller shouldn't need to worry about reference counts
    // or ownership.
    json_object_object_add(pObj, key, json_object_new_double(value));
}

void gdrive_json_add_boolean(Gdrive_Json_Object* pObj, 
                             const char* key, 
                             bool value
)
{
    // json_object_new_xxxx should give a reference count of 1, then
    // json_object_object_add should decrement the count.  Everything should
    // balance, and the caller shouldn't need to worry about reference counts
    // or ownership.
    json_object_object_add(pObj, key, json_object_new_boolean(value));
}

Gdrive_Json_Object* gdrive_json_add_new_array(Gdrive_Json_Object* pObj, 
                                              const char* key
)
{
    // json_object_new_xxxx should give a reference count of 1, then
    // json_object_object_add should decrement the count.  Everything should
    // balance, and the caller shouldn't need to worry about reference counts
    // or ownership.
    Gdrive_Json_Object* array = json_object_new_array();
    json_object_object_add(pObj, key, array);
    
    // Return the array in case the caller needs to add elements.  Don't do
    // anything to increment the reference count.  If the caller needs to keep
    // the array longer than the root object (which should be unusual), the 
    // caller can use gdrive_json_keep().
    return array;
}

void gdrive_json_add_existing_array(Gdrive_Json_Object* pObj,
                                                   const char* key,
                                                   Gdrive_Json_Object* pArray
)
{
    // Use json_object_get() before json_object_object_add() to leave the 
    // reference count unchanged.
    json_object_object_add(pObj, key, json_object_get(pArray));
}

const char* gdrive_json_to_string(Gdrive_Json_Object* pObj, bool pretty)
{
    int flags = pretty ? JSON_C_TO_STRING_PRETTY : JSON_C_TO_STRING_PLAIN;
    return json_object_to_json_string_ext(pObj, flags);
}

int gdrive_json_array_length(Gdrive_Json_Object* pObj, const char* key)
{
    Gdrive_Json_Object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL || !json_object_is_type(pInnerObj, json_type_array))
    {
        // Key not found or not an array, signal failure.
        return -1;
    }
    
    return json_object_array_length(pInnerObj);
}


Gdrive_Json_Object* gdrive_json_array_get(Gdrive_Json_Object* pObj, 
                                          const char* key, 
                                          int index
)
{
    Gdrive_Json_Object* pInnerObj = gdrive_json_get_nested_object(pObj, key);
    if (pInnerObj == NULL || !json_object_is_type(pInnerObj, json_type_array))
    {
        // Key not found, or object is not an array.  Return NULL for error.
        return NULL;
    }
    return json_object_array_get_idx(pInnerObj, index);
}

int gdrive_json_array_append_object(Gdrive_Json_Object* pArray, 
                                    Gdrive_Json_Object* pNewObj
)
{
    return json_object_array_add(pArray, pNewObj);
}

int gdrive_json_array_append_string(Gdrive_Json_Object* pArray, 
                                    const char* val
)
{
    // TODO: Check this and/or similar functions with valgrind to make sure they
    // don't leak memory. I'm pretty sure we don't need to do anything special 
    // with reference counts, but need to double check.
    
    return gdrive_json_array_append_object(pArray, json_object_new_string(val));
}

int gdrive_json_array_append_bool(Gdrive_Json_Object* pArray, bool val
)
{
    return gdrive_json_array_append_object(pArray, 
                                           json_object_new_boolean(val)
            );
}

int gdrive_json_array_append_double(Gdrive_Json_Object* pArray, double val
)
{
    return gdrive_json_array_append_object(pArray, json_object_new_double(val));
}

int gdrive_json_array_append_int64(Gdrive_Json_Object* pArray, int64_t val
)
{
    return gdrive_json_array_append_object(pArray, json_object_new_int64(val));
}


void gdrive_json_kill(Gdrive_Json_Object* pObj)
{
    json_object_put(pObj);
}

void gdrive_json_keep(Gdrive_Json_Object* pObj)
{
    json_object_get(pObj);
}

