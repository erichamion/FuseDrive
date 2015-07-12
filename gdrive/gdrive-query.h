/* 
 * File:   gdrive-query.h
 * Author: me
 * 
 * A struct and related functions for managing query strings or HTTP POST data.
 * Inspired by the curl_slist functions in libcurl.
 * 
 * This header is used internally by Gdrive code and should not be included 
 * outside of Gdrive code.
 *
 * Created on May 3, 2015, 7:33 PM
 */

#ifndef HEADER_TEMPLATE_H
#define	HEADER_TEMPLATE_H

// #ifdef	__cplusplus
// extern "C" {
// #endif
    
    
typedef struct Gdrive_Query Gdrive_Query;


/*************************************************************************
 * Constructors and destructors
 *************************************************************************/

/*
 * gdrive_query_free(): Frees all memory associated with a query.
 * Parameters:
 *      pQuery (Gdrive_Query*):
 *              A pointer to the struct to free. This pointer should no longer
 *              be used after this function returns. It is safe to pass a NULL
 *              pointer.
 */
void gdrive_query_free(Gdrive_Query* pQuery);



/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

// No getter or setter functions.


/*************************************************************************
 * Other accessible functions
 *************************************************************************/

/*
 * gdrive_query_add():  Creates a query with a given field and value, or adds a
 *                      field and value to an existing query.
 * Parameters:
 *      pQuery (Gdrive_Query*):
 *              If non-NULL, the query to which to add a field and value. If 
 *              NULL, a new query will be created.
 *      field (const char*):
 *              The field's name. The given string will be copied, so the caller
 *              can safely free the argument if desired. The copy will be 
 *              URL-escaped automatically.
 *      value (const char*):
 *              The value in the (field=value) pair. The given string will be 
 *              copied, so the caller can safely free the argument if desired. 
 *              The copy will be URL-escaped automatically.
 * Return value (int):
 *      A pointer to the query on success, or NULL on failure. If the pQuery
 *      argument was non-NULL and the function succeeds, the returned pointer
 *      will be the same as the pQuery argument. When the query struct is no 
 *      longer needed, the returned pointer should be passed to 
 *      gdrive_query_free().
 */
Gdrive_Query* gdrive_query_add(Gdrive_Query* pQuery, 
                               const char* field, 
                               const char* value
);

/*
 * gdrive_query_assemble(): Assembles HTTP POST data or a URL with a query 
 *                          string.
 * Parameters:
 *      pQuery (const Gdrive_Query*):
 *              Can be NULL or an empty query. Specifies the query to be added
 *              after the base URL, or the post data to be assembled.
 *      url (const char*):
 *              Can be NULL or empty string. Specifies the base URL on which to
 *              add the query.
 * Return value (char*):
 *      If BOTH pQuery and url are NULL, returns NULL.
 *      If BOTH pQuery and url are NON-NULL, returns a URL with queries, in 
 *      which the base URL part and the query part are separated by '?'. For
 *      example, if the url passed in is "http://www.example.com", then the
 *      returned string might be 
 *      "http://www.example.com?field1=value1&field2=value2".
 *      If ONLY pQuery is NULL, returns an exact copy of url.
 *      If ONLY url is NULL, returns the assembled query/post string. This is
 *      usable as HTTP POST data.
 *      NOTE 1: A NULL string or query is not the same as an empty string ("")
 *      or an empty query (a Gdrive_pQuery* that has been created but has not
 *      had any field/value pairs added). A NULL argument suppresses the 
 *      separating '?' character, whereas an empty argument does not.
 *      NOTE 2: The caller is responsible for freeing the memory pointed to by
 *      the return value.
 */
char* gdrive_query_assemble(const Gdrive_Query* pQuery, const char* url);



// #ifdef	__cplusplus
// }
// #endif

#endif	/* HEADER_TEMPLATE_H */

