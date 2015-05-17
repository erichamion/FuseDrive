/* 
 * File:   gdrive-transfer.h
 * Author: me
 * 
 * A struct and related functions to describe an upload or download request.
 *
 * Created on May 14, 2015, 7:33 PM
 */

#ifndef GDRIVE_TRANSFER_H
#define	GDRIVE_TRANSFER_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "gdrive-download-buffer.h"
    
#include <sys/types.h>
    
typedef struct Gdrive_Transfer Gdrive_Transfer;

typedef size_t (*gdrive_xfer_upload_callback)
(char* buffer, off_t offset, size_t size, void* userdata);


/*************************************************************************
 * Constructors and destructors
 *************************************************************************/

/*
 * gdrive_xfer_create():    Creates a new empty Gdrive_Transfer struct. Various
 *                          funtions in this header file should be used to add
 *                          options to the returned struct, and then the 
 *                          download or upload is performed with 
 *                          gdrive_xfer_execute().
 * Return value (Gdrive_Transfer*):
 *      On success, a pointer to a Gdrive_Transfer struct that can be used to
 *      describe and perform a download or upload. On failure, NULL.
 */
Gdrive_Transfer* gdrive_xfer_create();

/*
 * gdrive_xfer_free():  Safely frees the memory associated with a 
 *                      Gdrive_Transfer struct.
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):
 *              A pointer to the struct to be freed. After this function
 *              returns, the pointed-to memory should no longer be used. It is
 *              safe to pass a NULL pointer.
 */
void gdrive_xfer_free(Gdrive_Transfer* pTransfer);




/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

void gdrive_xfer_set_requesttype(Gdrive_Transfer* pTransfer, 
                                 enum Gdrive_Request_Type requestType
);

void gdrive_xfer_set_retryonautherror(Gdrive_Transfer* pTransfer, bool retry);

int gdrive_xfer_set_url(Gdrive_Transfer* pTransfer, const char* url);

void gdrive_xfer_set_destfile(Gdrive_Transfer* pTransfer, FILE* destFile);

void gdrive_xfer_set_uploadcallback(Gdrive_Transfer* pTransfer, 
                                    gdrive_xfer_upload_callback callback,
                                    void* userdata
);



/*************************************************************************
 * Other accessible functions
 *************************************************************************/

int gdrive_xfer_add_query(Gdrive_Transfer* pTransfer, 
                          const char* field, 
                          const char* value
);

int gdrive_xfer_add_postfield(Gdrive_Transfer* pTransfer, 
                          const char* field, 
                          const char* value
);

int gdrive_xfer_add_header(Gdrive_Transfer* pTransfer, const char* header);

Gdrive_Download_Buffer* gdrive_xfer_execute(Gdrive_Transfer* pTransfer);





#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_TRANSFER_DESCRIPTOR_H */

