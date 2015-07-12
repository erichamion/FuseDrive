
#include "gdrive-fileinfo-array.h"

#include <string.h>


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_Fileinfo_Array
{
    int nItems;
    int nMax;
    Gdrive_Fileinfo* pArray;
} Gdrive_Fileinfo_Array;

// No private functions


/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors, factory methods, destructors and similar
 ******************/

Gdrive_Fileinfo_Array* gdrive_finfoarray_create(int maxSize)
{
    Gdrive_Fileinfo_Array* pArray = malloc(sizeof(Gdrive_Fileinfo_Array));
    if (pArray != NULL)
    {
        size_t byteSize = maxSize * sizeof(Gdrive_Fileinfo);
        pArray->nItems = 0;
        pArray->nMax = maxSize;
        pArray->pArray = malloc(byteSize);
        if (pArray->pArray == NULL)
        {
            // Memory error
            free(pArray);
            return NULL;
        }
        memset(pArray->pArray, 0, byteSize);
    }
    // else memory error, do nothing and return NULL (the value of pArray).
    
    // Return a pointer to the new struct.
    return pArray;
}

void gdrive_finfoarray_free(Gdrive_Fileinfo_Array* pArray)
{
    if (pArray == NULL)
    {
        // Nothing to do
        return;
    }
    
    for (int i = 0; i < pArray->nItems; i++)
    {
        gdrive_finfo_cleanup(pArray->pArray + i);
    }
    
    if (pArray->nItems > 0)
    {
        free(pArray->pArray);
    }
    
    // Not really necessary, but doesn't harm anything
    pArray->nItems = 0;
    pArray->pArray = NULL;
    
    free(pArray);
}


/******************
 * Getter and setter functions
 ******************/

const Gdrive_Fileinfo* 
gdrive_finfoarray_get_first(Gdrive_Fileinfo_Array* pArray)
{
    return (pArray->nItems > 0) ? pArray->pArray : NULL;
}

const Gdrive_Fileinfo* 
gdrive_finfoarray_get_next(Gdrive_Fileinfo_Array* pArray, 
                           const Gdrive_Fileinfo* pPrev)
{
    if (pArray == NULL || pPrev == NULL)
    {
        // Invalid arguments
        return NULL;
    }
    const Gdrive_Fileinfo* pEnd = pArray->pArray + pArray->nMax;
    const Gdrive_Fileinfo* pNext = pPrev + 1;
    return (pNext < pEnd) ? pNext : NULL;
}

int gdrive_finfoarray_get_count(Gdrive_Fileinfo_Array* pArray)
{
    return pArray->nItems;
}

/******************
 * Other accessible functions
 ******************/

int gdrive_finfoarray_add_from_json(Gdrive_Fileinfo_Array* pArray, 
                                        Gdrive_Json_Object* pObj)
{
    if (pArray == NULL || pObj == NULL)
    {
        // Invalid parameters
        return -1;
    }
    if (pArray->nItems >= pArray->nMax)
    {
        // Too many items
        return -1;
    }
    
    // Read the info in, and increment nItems to show the new count.
    gdrive_finfo_read_json(pArray->pArray + pArray->nItems++, pObj);
    
    return 0;
    
}


/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

// No private functions