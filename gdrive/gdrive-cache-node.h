/* 
 * File:   gdrive-cache-node.h
 * Author: me
 * 
 * This file should be included only by modules that need direct access to the
 * cache nodes AS cache nodes (not by modules that only need to work with
 * Gdrive_Filehandle).
 * 
 * A struct and related functions to work with cached data for an individual
 * file.
 *
 * Created on May 4, 2015, 8:29 PM
 */

#ifndef GDRIVE_CACHE_NODE_H
#define	GDRIVE_CACHE_NODE_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "gdrive-file-contents.h"

    
typedef struct Gdrive_Cache_Node Gdrive_Cache_Node;

/*************************************************************************
 * Constructors and destructors
 *************************************************************************/

/*
 * gdrive_cnode_get():  Finds the cache node with the given fileId, optionally
 *                      creating it if it doesn't exist. (This is listed as a
 *                      constructor because it's the only public way to create
 *                      a new Gdrive_Cache_Node struct).
 * Parameters:
 *      pParent (Gdrive_Cache_Node*):
 *              Reserved for internal use. Must be NULL.
 *      ppNode (Gdrive_Cache_Node**):
 *              The address of a pointer to the root node. If the requested
 *              cache node doesn't already exist and addIfDoesntExist is true,
 *              this pointer may be changed.
 *      fileId (const char*):
 *              The Google Drive file ID to search for.
 *      addIfDoesntExist (bool):
 *              If a cache node doesn't already exist for the requested file ID,
 *              create a new one if and only if this argument is true.
 *      pAlreadyExists (bool*):
 *              Can be NULL. The address of a bool used to indicate whether the
 *              requested node had to be created or already existed. If 
 *              addIfDoesntExist is true, then the bool value at this memory
 *              location will become true if a new node was not created, and 
 *              false if a new node was created. If addIfDoesntExist is false,
 *              then the value stored here is undefined after this function 
 *              exits.
 * Return value (Gdrive_Cache_Node):
 *      On success, returns a pointer to a Gdrive_Cache_Node for the given
 *      Google Drive file ID. On failure, or if the given file ID doesn't 
 *      already have a cache node and addIfDoesntExist is false, returns NULL.
 */
Gdrive_Cache_Node* gdrive_cnode_get(Gdrive_Cache_Node* pParent,
                                    Gdrive_Cache_Node** ppNode,
                                    const char* fileId,
                                    bool addIfDoesntExist,
                                    bool* pAlreadyExists
);

/*
 *  gdrive_cnode_delete():  Deletes a node and safely frees its memory, 
 *                          preserving the structure of the remaining nodes.
 * Parameters:
 *      pNode (Gdrive_Cache_Node*):
 *              A pointer to the node to delete.
 *      ppToRoot (Gdrive_Cache_Node**):
 *              The address of a pointer to the root node. In some cases, this
 *              pointer may change.
 */
void gdrive_cnode_delete(Gdrive_Cache_Node* pNode, 
                         Gdrive_Cache_Node** ppToRoot
);

/*
 * gdrive_cnode_free_all(): Safely frees the memory associated with all cache
 *                          nodes in the tree.
 * Parameters:
 *      pRoot (Gdrive_Cache_Node*):
 *              A pointer to the root cache node.
 */
void gdrive_cnode_free_all(Gdrive_Cache_Node* pRoot);


/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

/*
 * gdrive_cnode_get_update_time():  Retrieve the time that a cache node was 
 *                                  last updated.
 * Parameters:
 *      pNode (Gdrive_Cache_Node*):
 *              A pointer to the cache node.
 * Return value (time_t):
 *      The time (in seconds since the epoch) that the cache node pointed to by
 *      pNode was last updated.
 */
time_t gdrive_cnode_get_update_time(Gdrive_Cache_Node* pNode);

/*
 * gdrive_cnode_get_filetype(): Retrieve the type of the file described by a
 *                              cache node.
 * Parameters:
 *      pNode (Gdrive_Cache_Node*):
 *              A pointer to the cache node.
 * Return value (enum Gdrive_Filetype):
 *      An enum value corresponding to the type of the file that the cache node
 *      pointed to by pNode describes. Current possible values are:
 *      GDRIVE_FILETYPE_FILE:   A regular file
        GDRIVE_FILETYPE_FOLDER: A folder (directory)
 *      See enum Gdrive_Filetype for the most up to date list of types.
 */
enum Gdrive_Filetype gdrive_cnode_get_filetype(Gdrive_Cache_Node* pNode);

/*
 * gdrive_cnode_get_fileinfo(): Retrieve the Gdrive_Fileinfo struct stored in a
 *                              cache node.
 * Parameters:
 *      pNode (Gdrive_Cache_Node*):
 *              A pointer to the cache node.
 * Return value (Gdrive_Fileinfo*):
 *      A pointer to the node's Gdrive_Fileinfo struct.
 */
Gdrive_Fileinfo* gdrive_cnode_get_fileinfo(Gdrive_Cache_Node* pNode);




/*************************************************************************
 * Other accessible functions
 *************************************************************************/

/*
 * gdrive_cnode_update_from_json(): Uses a JSON object to updates the file 
 *                                  information (size, modified time, etc.) 
 *                                  stored in a cache node, and sets the node's
 *                                  last updated time to the current time.
 * Parameters:
 *      pNode (Gdrive_Cache_Node*):
 *              A pointer to the cache node.
 *      pObj (gdrive_json_object*):
 *              A pointer to a JSON object. This object should hold a Google
 *              Drive files resource as described at 
 *              https://developers.google.com/drive/v2/reference/files
 */
void gdrive_cnode_update_from_json(Gdrive_Cache_Node* pNode, 
                                       Gdrive_Json_Object* pObj
);

/*
 * gdrive_cnode_delete_file_contents(): Remove a single Gdrive_File_Contents
 *                                      struct (describing and holding a FILE*
 *                                      handle for a single portion of the file)
 *                                      from a cache node and safely frees its
 *                                      memory.
 * Parameters:
 *      pNode (Gdrive_Cache_Node*):
 *              A pointer to the cache node.
 *      pContents (Gdrive_File_Contents*):
 *              A pointer to the struct to be deleted.
 */
void gdrive_cnode_delete_file_contents(Gdrive_Cache_Node* pNode, 
                                Gdrive_File_Contents* pContents
);



#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_CACHE_NODE_H */

