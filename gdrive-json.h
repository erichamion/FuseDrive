/* 
 * File:   gdrive-json.h
 * Author: me
 * 
 * A thin wrapper around the json-c library
 *
 * Created on April 15, 2015, 1:20 AM
 */

#ifndef GDRIVE_JSON_H
#define	GDRIVE_JSON_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <json-c/json.h>
    
typedef json_object gdrive_json_object;


gdrive_json_object* gdrive_json_get_nested_object(gdrive_json_object* pObj, 
                                                  const char* key
);

int gdrive_json_get_string(gdrive_json_object* pObj, 
                           const char* key, 
                           char* result, 
                           int maxlen
);

int64_t gdrive_json_get_int64(gdrive_json_object* pObj, 
                           const char* key, 
                           bool* pSuccess
);

double gdrive_json_get_double(gdrive_json_object* pObj, 
                           const char* key, 
                           bool* pSuccess
);

bool gdrive_json_get_boolean(gdrive_json_object* pObj, 
                           const char* key, 
                           bool* pSuccess
);

int gdrive_json_array_length(gdrive_json_object* pObj, const char* key);

gdrive_json_object* gdrive_json_array_get(gdrive_json_object* pObj, 
                                          const char* key, 
                                          int index
);

gdrive_json_object* gdrive_json_from_str(const char* inStr);

void gdrive_json_kill(gdrive_json_object* pObj);

void gdrive_json_keep(gdrive_json_object* pObj);



#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_JSON_H */

