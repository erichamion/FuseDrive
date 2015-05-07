

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "gdrive-fileid-cache-node.h"


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_Fileid_Cache_Node
{
    time_t lastUpdateTime;
    char* path;
    char* fileId;
    struct Gdrive_Fileid_Cache_Node* pNext;
} Gdrive_Fileid_Cache_Node;

static Gdrive_Fileid_Cache_Node* 
gdrive_fidnode_create(const char* filename, const char* fileId);

static int 
gdrive_fidnode_update_item(Gdrive_Fileid_Cache_Node* pNode, 
                                 const char* fileId);

static void 
gdrive_fidnode_free(Gdrive_Fileid_Cache_Node* pNode);



/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors and destructors
 ******************/

int gdrive_fidnode_add(Gdrive_Fileid_Cache_Node** ppHead,
                                  const char* path,
                                  const char* fileId
)
{
    if (*ppHead == NULL)
    {
        // Add the current element as the first one in the list.
        *ppHead = gdrive_fidnode_create(path, fileId);
        if (*ppHead == NULL)
        {
            // Error, probably memory
            return -1;
        }
        // else return success
        return 0;
    }
    
    
    Gdrive_Fileid_Cache_Node** ppFromPrev = ppHead;
    Gdrive_Fileid_Cache_Node* pNext = *ppFromPrev;

    
    while (true)
    {
        //Gdrive_Fileid_Cache_Node* pNext = pPrev->pNext;
        // Find the string comparison.  If pNext is NULL, pretend pNext->path
        // is greater than path (we insert after pPrev in both cases).
        int cmp = (pNext != NULL) ? strcmp(path, pNext->path) : -1;
        
        if (cmp == 0)
        {
            // Item already exists, update it.
            return gdrive_fidnode_update_item(pNext, fileId);
        }
        else if (cmp < 0)
        {
            // Item doesn't exist yet, insert it between pPrev and pNext.
            Gdrive_Fileid_Cache_Node* pNew = 
                    gdrive_fidnode_create(path, fileId);
            if (pNew == NULL)
            {
                // Error, most likely memory.
                return -1;
            }
            *ppFromPrev = pNew;
            pNew->pNext = pNext;
            return 0;
        }
        // else keep searching
        *ppFromPrev = (*ppFromPrev)->pNext;
        pNext = *ppFromPrev;
    }
}

void gdrive_fidnode_remove_by_id(Gdrive_Fileid_Cache_Node* pHead, 
                                    const char* fileId
)
{
    // Need to walk through the whole list, since it's not keyed by fileId.
    Gdrive_Fileid_Cache_Node* pPrev = pHead;
    Gdrive_Fileid_Cache_Node* pNext = pPrev->pNext;
    
    while (pNext != NULL)
    {
        
        // Compare the given fileId to the current one.
        int cmp = strcmp(fileId, pNext->fileId);
        
        if (cmp == 0)
        {
            // Found it!
            pPrev->pNext = pNext->pNext;
            gdrive_fidnode_free(pNext);
            
            // Still need to keep searching, since one file ID can correspond
            // to many paths.
        }
        // else keep searching
        pPrev = pNext;
        pNext = pPrev->pNext;
    }
}

void gdrive_fidnode_clear_all(Gdrive_Fileid_Cache_Node* pHead)
{
    // Free the rest of the chain.
    if (pHead->pNext != NULL)
    {
        gdrive_fidnode_clear_all(pHead->pNext);
        gdrive_fidnode_free(pHead->pNext);
    }
    
    // Free the head node.
    gdrive_fidnode_free(pHead);
}




/******************
 * Getter and setter functions
 ******************/

time_t gdrive_fidnode_get_lastupdatetime(Gdrive_Fileid_Cache_Node* pNode)
{
    return pNode->lastUpdateTime;
}

const char* gdrive_fidnode_get_fileid(Gdrive_Fileid_Cache_Node* pNode)
{
    return pNode->fileId;
}


/******************
 * Other accessible functions
 ******************/

Gdrive_Fileid_Cache_Node* gdrive_fidnode_get_node(
        Gdrive_Fileid_Cache_Node* pHead, 
        const char* path
)
{
    if (pHead == NULL)
    {
        // No nodes to get.
        return NULL;
    }
    Gdrive_Fileid_Cache_Node* pNode = pHead->pNext;
    
    while (pNode != NULL)
    {
        int cmp = strcmp(path, pNode->path);
        if (cmp == 0)
        {
            // Found it!
            return pNode;
        }
        else if (cmp < 0)
        {
            // We've gone too far.  It's not here.
            return NULL;
        }
        // else keep searching
        pNode = pNode->pNext;
    }
    
    // We've hit the end of the list without finding path.
    return NULL;
}



/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

static Gdrive_Fileid_Cache_Node* 
gdrive_fidnode_create(const char* filename, const char* fileId)
{
    Gdrive_Fileid_Cache_Node* pResult = 
            malloc(sizeof(Gdrive_Fileid_Cache_Node));
    if (pResult != NULL)
    {
        memset(pResult, 0, sizeof(Gdrive_Fileid_Cache_Node));
        
        if (filename != NULL)
        {
            pResult->path = malloc(strlen(filename) + 1);
            if (pResult->path == NULL)
            {
                // Memory error.
                free(pResult);
                return NULL;
            }
            strcpy(pResult->path, filename);
        }
        
        // Only try copying the fileId if it was specified.
        if (fileId != NULL)
        {
            pResult->fileId = malloc(strlen(fileId) + 1);
            if (pResult->fileId == NULL)
            {
                // Memory error.
                free(pResult->path);
                free(pResult);
                return NULL;
            }
            strcpy(pResult->fileId, fileId);
        }
        
        // Set the updated time.
        pResult->lastUpdateTime = time(NULL);
    }
    return pResult;
}

static int 
gdrive_fidnode_update_item(Gdrive_Fileid_Cache_Node* pNode, 
                                 const char* fileId)
{
    // Update the time.
    pNode->lastUpdateTime = time(NULL);
    
    if ((pNode->fileId == NULL) || (strcmp(fileId, pNode->fileId) != 0))
    {
        // pNode doesn't have a fileId or the IDs don't match. Copy the new
        // fileId in.
        free(pNode->fileId);
        pNode->fileId = malloc(strlen(fileId) + 1);
        if (pNode->fileId == NULL)
        {
            // Memory error.
            return -1;
        }
        strcpy(pNode->fileId, fileId);
        return 0;
    }
    // else the IDs already match.
    return 0;
}

/*
 * DOES NOT REMOVE FROM LIST.  FREES ONLY THE SINGLE NODE.
 */
static void 
gdrive_fidnode_free(Gdrive_Fileid_Cache_Node* pNode)
{
    free(pNode->fileId);
    pNode->fileId = NULL;
    free(pNode->path);
    pNode->path = NULL;
    pNode->pNext = NULL;
    free(pNode);
}

