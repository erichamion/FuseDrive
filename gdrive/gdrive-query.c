

#include "gdrive-query.h"
#include "gdrive-info.h"

#include <stdlib.h>
#include <string.h>



/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_Query
{
    char* field;
    char* value;
    struct Gdrive_Query* pNext;
} Gdrive_Query;

Gdrive_Query* gdrive_query_create(void);


/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors, factory methods, destructors and similar
 ******************/

void gdrive_query_free(Gdrive_Query* pQuery)
{
    if (pQuery == NULL)
    {
        // Nothing to do
        return;
    }
    
    // Work from the end back to the beginning with recursion.
    if (pQuery->pNext != NULL)
    {
        gdrive_query_free(pQuery->pNext);
    }
    
    // Free up the strings.  They were created with curl_easy_escape(), so we 
    // need to use curl_free().
    if (pQuery->field != NULL)
    {
        curl_free(pQuery->field);
    }
    if (pQuery->value != NULL)
    {
        curl_free(pQuery->value);
    }
    
    // Free the struct itself.
    free(pQuery);
}


/******************
 * Getter and setter functions
 ******************/

// No getter or setter functions


/******************
 * Other accessible functions
 ******************/

Gdrive_Query* gdrive_query_add(Gdrive_Query* pQuery, 
                               const char* field, 
                               const char* value
)
{
    // TODO: Don't kill the entire list of queries if something goes wrong. Just
    // free any newly-created query from the end of the list, restoring things
    // to the same state as before this function was called. Doing this will
    // require a different mechanism to report errors, can't just return NULL
    // (and have the caller lose the pointer to the original Gdrive_Query list).
    
    // If there is no existing Gdrive_Query, create an empty one.
    if (pQuery == NULL)
    {
        pQuery = gdrive_query_create();
        if (pQuery == NULL)
        {
            // Memory error
            return NULL;
        }
    }
    
    // Add a new empty Gdrive_Query to the end of the list (unless the first
    // one is already empty, as it will be if we just created it).
    Gdrive_Query* pLast = pQuery;
    if (pLast->field != NULL || pLast->value != NULL)
    {
        while (pLast->pNext != NULL)
        {
            pLast = pLast->pNext;
        }
        pLast->pNext = gdrive_query_create();
        if (pLast->pNext == NULL)
        {
            // Memory error
            gdrive_query_free(pQuery);
            return NULL;
        }
        pLast = pLast->pNext;
    }
    
    // Populate the newly-created Gdrive_Query with URL-escaped strings
    CURL* curlHandle = gdrive_get_curlhandle();
    if (curlHandle == NULL)
    {
        // Error
        gdrive_query_free(pQuery);
        return NULL;
    }
    pLast->field = curl_easy_escape(curlHandle, field, 0);
    pLast->value = curl_easy_escape(curlHandle, value, 0);
    curl_easy_cleanup(curlHandle);
    
    if (pLast->field == NULL || pLast->value == NULL)
    {
        // Error
        gdrive_query_free(pQuery);
        return NULL;
    }
    return pQuery;
}



char* gdrive_query_assemble(const Gdrive_Query* pQuery, const char* url)
{
    // If there is a url, allow for its length plus the '?' character (or the
    // url length plus terminating null if there is no query string).
    int totalLength = (url == NULL) ? 0 : (strlen(url) + 1);
    
    // If there is a query string (or POST data, which is handled the same way),
    // each field adds its length plus 1 for the '=' character. Each value adds
    // its length plus 1, for either the '&' character (all but the last item)
    // or the terminating null (on the last item).
    const Gdrive_Query* pCurrentQuery = pQuery;
    while (pCurrentQuery != NULL)
    {
        if (pCurrentQuery->field != NULL && pCurrentQuery->value != NULL)
        {
            totalLength += strlen(pCurrentQuery->field) + 1;
            totalLength += strlen(pCurrentQuery->value) + 1;
        }
        else
        {
            // The query is empty.  Add 1 to the length for the null terminator
            totalLength++;
        }
        pCurrentQuery = pCurrentQuery->pNext;
    }
    
    if (totalLength < 1)
    {
        // Invalid arguments
        return NULL;
    }
    
    // Allocate a string long enough to hold everything.
    char* result = malloc(totalLength);
    if (result == NULL)
    {
        // Memory error
        return NULL;
    }
    
    // Copy the url into the result string.  If there is no url, start with an
    // empty string.
    if (url != NULL)
    {
        strcpy(result, url);
        if (pQuery == NULL)
        {
            // We had a URL string but no query string, so we're done.
            return result;
        }
        else
        {
            // We have both a URL and a query, so they need to be separated
            // with a '?'.
            strcat(result, "?");
        }
    }
    else
    {
        result[0] = '\0';
    }
    
    if (pQuery == NULL)
    {
        // There was no query string, so we're done.
        return result;
    }
    
    // Copy each of the query field/value pairs into the result.
    pCurrentQuery = pQuery;
    while (pCurrentQuery != NULL)
    {
        if (pCurrentQuery->field != NULL && pCurrentQuery->value != NULL)
        {
            strcat(result, pCurrentQuery->field);
            strcat(result, "=");
            strcat(result, pCurrentQuery->value);
        }
        if (pCurrentQuery->pNext != NULL)
        {
            strcat(result, "&");
        }
        pCurrentQuery = pCurrentQuery->pNext;
    }
    
    return result;
}

/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

Gdrive_Query* gdrive_query_create(void)
{
    Gdrive_Query* result = malloc(sizeof(Gdrive_Query));
    if (result != NULL)
    {
        memset(result, 0, sizeof(Gdrive_Query));
    }
    return result;
}
