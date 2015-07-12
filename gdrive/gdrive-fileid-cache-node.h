/* 
 * File:   gdrive-fileid-cache-node.h
 * Author: me
 * 
 * A struct and related functions for managing nodes in a cache that maps from
 * file paths to Google Drive file IDs.
 * 
 * This header is used internally by Gdrive code and should not be included 
 * outside of Gdrive code.
 *
 * Created on May 7, 2015, 1:49 AM
 */

#ifndef GDRIVE_FILEID_CACHE_NODE_H
#define	GDRIVE_FILEID_CACHE_NODE_H

// #ifdef	__cplusplus
// extern "C" {
// #endif
    
#include <time.h>
    
typedef struct Gdrive_Fileid_Cache_Node Gdrive_Fileid_Cache_Node;

/*************************************************************************
 * Constructors, factory methods, destructors and similar
 *************************************************************************/

/*
 * gdrive_fidnode_add():    Adds a file ID cache node to an existing tree if it
 *                          doesn't already exist in the tree, or update the
 *                          fileId if a node for the path already exists.
 * Parameters:
 *      pHead (Gdrive_Fileid_Cache_Node**):
 *              The address of a pointer to the root of the File ID cache node
 *              tree.
 *      path (const char*):
 *              The filepath of the node to add or update.
 *      fileId (const char*):
 *              The Google Drive file ID of the node to add or update.
 * Return value (int):
 *      0 on success, other on error.
 */
int gdrive_fidnode_add(Gdrive_Fileid_Cache_Node** pHead, 
                                          const char* path,
                                          const char* fileId
);

/*
 * gdrive_fidnode_remove_by_id():   Finds any file ID nodes containing a given
 *                                  Google Drive file ID, safely removes 
 *                                  them from the list of nodes, and safely
 *                                  frees any memory associated with them.
 * Parameters:
 *      pHead (Gdrive_Fileid_Cache_Node*):
 *              A pointer to the first node in the list.
 *      fileId (const char*):
 *              The Google Drive file ID to search for and remove.
 */
void gdrive_fidnode_remove_by_id(Gdrive_Fileid_Cache_Node** ppHead, 
                                 const char* fileId
);

/*
 * gdrive_fidnode_clear_all():  Safely frees the memory associated with an 
 *                              entire list of file ID nodes.
 * Parameters:
 *      pHead (Gdrive_Fileid_Cache_Node*):
 *              A pointer to the head node in the list to be freed.
 */
void gdrive_fidnode_clear_all(Gdrive_Fileid_Cache_Node* pHead);



/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

/*
 * gdrive_fidnode_get_lastupdatetime(): Retrieve the time (in seconds since the
 *                                      epoch) that the given file ID node was
 *                                      last updated with fresh information from
 *                                      Google Drive.
 * Parameters:
 *      pNode (Gdrive_Fileid_Cache_Node*):
 *              The file ID node from which to retrieve the updated time.
 * Return value (time_t):
 *      The last updated time.
 */
time_t gdrive_fidnode_get_lastupdatetime(Gdrive_Fileid_Cache_Node* pNode);

/*
 * gdrive_fidnode_get_fileid(): Retrieves a new copy of the Google Drive file ID
 *                              stored in a specified file ID cache node.
 * Parameters:
 *      pNode (Gdrive_Fileid_Cache_Node*):
 *              A pointer to the node.
 * Return value (const char*):
 *      The stored Google Drive file ID as a null-terminated string. The caller
 *      is responsible for freeing the memory at the pointed-to location.
 */
char* gdrive_fidnode_get_fileid(Gdrive_Fileid_Cache_Node* pNode);


/*************************************************************************
 * Other accessible functions
 *************************************************************************/

/*
 * gdrive_fidnode_get_node():   Find the file ID node holding the given 
 *                              filepath.
 * Parameters:
 *      pHead (Gdrive_Fileid_Cache_Node*):
 *              A pointer to the head node in the list of nodes to search.
 *      path (const char*):
 *              The filepath to search for.
 * Return value (Gdrive_Fileid_Cache_Node*):
 *      A pointer to the file ID node containing the given path, if such a node
 *      exists. Otherwise, returns 0.
 */
Gdrive_Fileid_Cache_Node* gdrive_fidnode_get_node(
        Gdrive_Fileid_Cache_Node* pHead, 
        const char* path
);





// #ifdef	__cplusplus
// }
// #endif

#endif	/* GDRIVE_FILEID_CACHE_NODE_H */

