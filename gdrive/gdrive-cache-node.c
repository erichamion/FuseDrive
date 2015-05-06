

#include <sys/types.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <curl/curl.h>

#include "gdrive.h"
#include "gdrive-json.h"
#include "gdrive-internal.h"
#include "gdrive-cache-node.h"
#include "gdrive-file.h"


/*************************************************************************
 * Private struct and declarations of private functions for use within 
 * this file
 *************************************************************************/

typedef struct Gdrive_Cache_Node
{
    time_t lastUpdateTime;
    int openReads;
    int openWrites;
    int openOthers;
    Gdrive_Fileinfo fileinfo;
    Gdrive_File_Contents* pContents;
    struct Gdrive_Cache_Node* pParent;
    struct Gdrive_Cache_Node* pLeft;
    struct Gdrive_Cache_Node* pRight;
} Gdrive_Cache_Node;

static Gdrive_Cache_Node* gdrive_cnode_create(Gdrive_Cache_Node* pParent);

static void
gdrive_cnode_swap(Gdrive_Cache_Node** ppFromParentOne,
                        Gdrive_Cache_Node* pNodeOne,
                        Gdrive_Cache_Node** ppFromParentTwo,
                        Gdrive_Cache_Node* pNodeTwo
);

static void
gdrive_cnode_free(Gdrive_Cache_Node* pNode);




/*************************************************************************
 * Implementations of public functions for internal or external use
 *************************************************************************/

/******************
 * Constructors and destructors
 ******************/

Gdrive_Cache_Node* gdrive_cnode_get(Gdrive_Cache_Node* pParent,
                                    Gdrive_Cache_Node** ppNode,
                                    const char* fileId,
                                    bool addIfDoesntExist,
                                    bool* pAlreadyExists
)
{
    if (pAlreadyExists != NULL)
    {
        *pAlreadyExists = false;
    }
    
    if (*ppNode == NULL)
    {
        // Item doesn't exist in the cache. Either fail, or create a new item.
        if (!addIfDoesntExist)
        {
            // Not allowed to create a new item, return failure.
            return NULL;
        }
        // else create a new item.
        *ppNode = gdrive_cnode_create(pParent);
        if (*ppNode != NULL)
        {
            // Convenience to avoid things like "return &((*ppNode)->fileinfo);"
            Gdrive_Cache_Node* pNode = *ppNode;
            
            // Copy the fileId into the fileinfo. Everything else is left null.
            pNode->fileinfo.id = malloc(strlen(fileId) + 1);
            if (pNode->fileinfo.id == NULL)
            {
                // Memory error.
                gdrive_cnode_free(pNode);
                *ppNode = NULL;
                return NULL;
            }
            strcpy(pNode->fileinfo.id, fileId);
            
            // Since this is a new entry, set the node's updated time.
            pNode->lastUpdateTime = time(NULL);
            
            return pNode;
        }
    }
    
    // Convenience to avoid things like "&((*ppNode)->pRight)"
    Gdrive_Cache_Node* pNode = *ppNode;
    
    // Root node exists, try to find the fileId in the tree.
    int cmp = strcmp(fileId, pNode->fileinfo.id);
    if (cmp == 0)
    {
        // Found it at the current node.
        if (pAlreadyExists != NULL)
        {
            *pAlreadyExists = true;
        }
        return pNode;
    }
    else if (cmp < 0)
    {
        // fileId is less than the current node. Look for it on the left.
        return gdrive_cnode_get(pNode, &(pNode->pLeft), fileId, 
                                      addIfDoesntExist, pAlreadyExists
                );
    }
    else
    {
        // fileId is greater than the current node. Look for it on the right.
        return gdrive_cnode_get(pNode, &(pNode->pRight), fileId, 
                                      addIfDoesntExist, pAlreadyExists
                );
    }
}


void gdrive_cnode_delete(Gdrive_Cache_Node* pNode, 
                               //Gdrive_Cache_Node* pParentNode,
                               Gdrive_Cache_Node** ppToRoot
)
{
    // The address of the pointer aimed at this node. If this is the root node,
    // then it will be a pointer passed in from outside. Otherwise, it is the
    // pLeft or pRight member of the parent.
    Gdrive_Cache_Node** ppFromParent;
    if (pNode->pParent == NULL)
    {
        // This is the root. Take the pointer that was passed in.
        ppFromParent = ppToRoot;
    }
    else
    {
        // Not the root. Find whether the node hangs from the left or right side
        // of its parent.
        ppFromParent = (pNode->pParent->pLeft == pNode) ?
            &(pNode->pParent->pLeft) : &(pNode->pParent->pRight);
        
    }
    
    // Simplest special case. pNode has no descendents.  Just delete it, and
    // set the pointer from the parent to NULL.
    if (pNode->pLeft == NULL && pNode->pRight == NULL)
    {
        *ppFromParent = NULL;
        gdrive_cnode_free(pNode);
        return;
    }
    
    // Second special case. pNode has one side empty. Promote the descendent on
    // the other side into pNode's place.
    if (pNode->pLeft == NULL)
    {
        *ppFromParent = pNode->pRight;
        pNode->pRight->pParent = pNode->pParent;
        gdrive_cnode_free(pNode);
        return;
    }
    if (pNode->pRight == NULL)
    {
        *ppFromParent = pNode->pLeft;
        pNode->pLeft->pParent = pNode->pParent;
        gdrive_cnode_free(pNode);
        return;
    }
    
    // General case with descendents on both sides. Find the node with the 
    // closest value to pNode in one of its subtrees (leftmost node of the right
    // subtree, or rightmost node of the left subtree), and switch places with
    // pNode.  Which side we use doesn't really matter.  We'll rather 
    // arbitrarily decide to use the same side subtree as the side from which
    // pNode hangs off its parent (if pNode is on the right side of its parent,
    // find the leftmost node of the right subtree), and treat the case where
    // pNode is the root the same as if it were on the left side of its parent.
    Gdrive_Cache_Node* pSwap = NULL;
    Gdrive_Cache_Node** ppToSwap = NULL;
    if (pNode->pParent != NULL && pNode->pParent->pRight == pNode)
    {
        // Find the leftmost node of the right subtree.
        pSwap = pNode->pRight;
        ppToSwap = &(pNode->pRight);
        while (pSwap->pLeft != NULL)
        {
            ppToSwap = &(pSwap->pLeft);
            pSwap = pSwap->pLeft;
        }
    }
    else
    {
        // Find the rightmost node of the left subtree.
        pSwap = pNode->pLeft;
        ppToSwap = &(pNode->pLeft);
        while (pSwap->pRight != NULL)
        {
            ppToSwap = &(pSwap->pRight);
            pSwap = pSwap->pRight;
        }
    }
    
    // Swap the nodes
    gdrive_cnode_swap(ppFromParent, pNode, ppToSwap, pSwap);
    
//    // Find the pointer from pNode's new parent.  We don't need to worry about
//    // a NULL pParent, since pNode can't be at the root of the tree after
//    // swapping.
//    Gdrive_Cache_Node** ppFromNewParent = (pNode->pParent->pLeft == pNode ?
//        &(pNode->pParent->pLeft) :
//        &(pNode->pParent->pRight)
//            );
    
    // Now delete the node from its new position.
    gdrive_cnode_delete(pNode, ppToRoot);
}

void gdrive_cnode_free_all(Gdrive_Cache_Node* pRoot)
{
    if (pRoot == NULL)
    {
        // Nothing to do.
        return;
    }
    
    // Free all the descendents first.
    gdrive_cnode_free_all(pRoot->pLeft);
    gdrive_cnode_free_all(pRoot->pRight);
    
    // Free the root node
    gdrive_cnode_free(pRoot);
}


/******************
 * Getter and setter functions
 ******************/

time_t gdrive_cnode_get_update_time(Gdrive_Cache_Node* pNode)
{
    return pNode->lastUpdateTime;
}

enum Gdrive_Filetype gdrive_cnode_get_filetype(Gdrive_Cache_Node* pNode)
{
    return pNode->fileinfo.type;
}

Gdrive_Fileinfo* gdrive_cnode_get_fileinfo(Gdrive_Cache_Node* pNode)
{
    // TODO: Make the fileinfo structure private, and provide functions that
    // take a Gdrive_Filehandle* argument to provide all the fileinfo 
    // information.
    
    return &(pNode->fileinfo);
}

Gdrive_File_Contents* gdrive_cnode_get_contents(Gdrive_Cache_Node* pNode)
{
    return pNode->pContents;
}

bool gdrive_cnode_set_contents(Gdrive_Cache_Node* pNode,
                                    Gdrive_File_Contents* pContents
)
{
    bool returnVal = false;
    
    if (pNode->pContents != NULL)
    {
        // A non-NULL pContents pointer already exists and has not been cleared.
        // Clear the old one first and prepare to return true.
        _gdrive_file_contents_free_all(&(pNode->pContents));
        returnVal = true;
    }
    
    pNode->pContents = pContents;
    return returnVal;
}

Gdrive_Cache_Node* gdrive_cache_node_get_parent(Gdrive_Cache_Node* pNode)
{
    return pNode->pParent;
}


/******************
 * Other accessible functions
 ******************/

void gdrive_cnode_update_from_json(Gdrive_Cache_Node* pNode, 
                                       gdrive_json_object* pObj
)
{
    if (pNode == NULL || pObj == NULL)
    {
        // Nothing to do
        return;
    }
    gdrive_fileinfo_cleanup(&(pNode->fileinfo));
    _gdrive_get_fileinfo_from_json(pObj, &(pNode->fileinfo));
    
    // Mark the node as having been updated.
    pNode->lastUpdateTime = time(NULL);
}

void gdrive_cnode_delete_file_contents(Gdrive_Cache_Node* pNode, 
                                Gdrive_File_Contents* pContents
)
{
    // Find the pointer leading to pContents.
    Gdrive_File_Contents** ppContents = &(pNode->pContents);
    while (*ppContents != NULL && *ppContents != pContents)
    {
        ppContents = &((*ppContents)->pNext);
    }
    
    // Take pContents out of the chain
    if (*ppContents != NULL)
    {
        *ppContents = pContents->pNext;
    }
    
    // Close the temp file
    if (pContents->fh != NULL)
    {
        fclose(pContents->fh);
        pContents->fh = NULL;
    }
    
    free(pContents);
}


/*************************************************************************
 * Public functions to support Gdrive_Filehandle usage
 *************************************************************************/

Gdrive_Filehandle* gdrive_file_open(Gdrive_Info* pInfo, 
                                    const char* fileId,
                                    int flags
)
{
    // Get the cache node from the cache if it exists.  If it doesn't exist,
    // don't make a node with an empty Gdrive_Fileinfo.  Instead, use 
    // gdrive_file_info_from_id() to create the node and fill out the struct, 
    // then try again to get the node.
    Gdrive_Cache_Node* pNode;
    while ((pNode = 
            gdrive_cache_get_node(pInfo->pInternalInfo->pCache,
                                  fileId, false, NULL)
            ) == NULL)
    {
        Gdrive_Fileinfo* pDummy;
        if (gdrive_file_info_from_id(pInfo, fileId, &pDummy) != 0)
        {
            // Problem getting the file info.  Return failure.
            return NULL;
        }
    }
    
    // Don't open directories, only regular files.
    if (pNode->fileinfo.type == GDRIVE_FILETYPE_FOLDER)
    {
        // Return failure
        return NULL;
    }
    
    // Increment open file counts.
    if ((flags & O_RDONLY) || (flags & O_RDWR))
    {
        // Open for reading (not necessarily only reading)
        pNode->openReads++;
    }
    if ((flags & O_WRONLY) || (flags & O_RDWR))
    {
        // Open for writing (not necessarily only writing)
        pNode->openWrites++;
    }
    if (!(flags & (O_RDONLY | O_WRONLY | O_RDWR)))
    {
        // Some opens don't have any of these flags. I'm not sure what these
        // are.
        pNode->openOthers++;
    }
    
    // Return a pointer to the cache node (which is typedef'ed to 
    // Gdrive_Filehandle)
    return pNode;
    
}

void gdrive_file_close(Gdrive_Filehandle* pFile, int flags)
{
    Gdrive_Cache_Node* pNode = pFile;
    
    // Decrement open file counts.
    if ((flags & O_RDONLY) || (flags & O_RDWR))
    {
        // Was opened for reading (not necessarily only reading)
        pNode->openReads--;
    }
    if ((flags & O_WRONLY) || (flags & O_RDWR))
    {
        // Was opened for writing (not necessarily only writing)
        pNode->openWrites--;
        
        // TODO: Upload the new version of the file to the Google Drive servers
    }
    if (!(flags & (O_RDONLY | O_WRONLY | O_RDWR)))
    {
        // Some opens don't have any of these flags. I'm not sure what these
        // are.
        pNode->openOthers--;
    }
    
    // Get rid of any downloaded temp files if they aren't needed.
    // TODO: Consider keeping some closed files around in case they're reopened
    if (pNode->openReads + pNode->openWrites + pNode->openOthers == 0)
    {
        _gdrive_file_contents_free_all(&(pNode->pContents));
    }
}


/*************************************************************************
 * Implementations of private functions for use within this file
 *************************************************************************/

/*
 * Set pParent to NULL for the root node of the tree (the node that has no
 * parent).
 */
static Gdrive_Cache_Node* gdrive_cnode_create(Gdrive_Cache_Node* pParent)
{
    Gdrive_Cache_Node* result = malloc(sizeof(Gdrive_Cache_Node));
    if (result != NULL)
    {
        memset(result, 0, sizeof(Gdrive_Cache_Node));
        result->pParent = pParent;
    }
    return result;
}

static void
gdrive_cnode_swap(Gdrive_Cache_Node** ppFromParentOne,
                        Gdrive_Cache_Node* pNodeOne,
                        Gdrive_Cache_Node** ppFromParentTwo,
                        Gdrive_Cache_Node* pNodeTwo
)
{
    // Swap the pointers from the parents
    *ppFromParentOne = pNodeTwo;
    *ppFromParentTwo = pNodeOne;
    
    Gdrive_Cache_Node* pTempParent = pNodeOne->pParent;
    Gdrive_Cache_Node* pTempLeft = pNodeOne->pLeft;
    Gdrive_Cache_Node* pTempRight = pNodeOne->pRight;
    
    pNodeOne->pParent = pNodeTwo->pParent;
    pNodeOne->pLeft = pNodeTwo->pLeft;
    pNodeOne->pRight = pNodeTwo->pRight;
    
    pNodeTwo->pParent = pTempParent;
    pNodeTwo->pLeft = pTempLeft;
    pNodeTwo->pRight = pTempRight;
}

/*
 * NOT RECURSIVE.  FREES ONLY THE SINGLE NODE.
 */
static void
gdrive_cnode_free(Gdrive_Cache_Node* pNode)
{
    gdrive_fileinfo_cleanup(&(pNode->fileinfo));
    _gdrive_file_contents_free_all(&(pNode->pContents));
    pNode->pContents = NULL;
    pNode->pLeft = NULL;
    pNode->pRight = NULL;
    free(pNode);
}



