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

/*
 * gdrive_xfer_upload_callback: Signature for a callback function to be used
 *                              with gdrive_xfer_set_uploadcallback().
 * Parameters:
 *      buffer (char*): 
 *              A memory buffer into which the callback function must fill data 
 *              (not necessarily at the start of the buffer -- see the offset 
 *              parameter).
 *      offset (off_t): 
 *              The offset from the start of the buffer at which to start 
 *              placing data.
 *      size (size_t):  
 *              The maximum number of bytes to place into the buffer.
 *      userdata (void*):   
 *              Any user-defined state or other information that may be needed 
 *              by the callback function.
 * Notes:
 *      For more information on when and how this callback function will be 
 *      called, see the libcurl documentation for CURLOPT_READFUNCTION, which
 *      is very similar (although the function arguments are slightly 
 *      different).
 */
typedef size_t (*gdrive_xfer_upload_callback)
(char* buffer, off_t offset, size_t size, void* userdata);


/*************************************************************************
 * Constructors, factory methods, destructors and similar
 *************************************************************************/

/*
 * gdrive_xfer_create():    Creates a new empty Gdrive_Transfer struct. Various
 *                          funtions in this header file should be used to add
 *                          options to the returned struct, and then the 
 *                          download or upload is performed with 
 *                          gdrive_xfer_execute().
 * Return value (Gdrive_Transfer*):
 *      On success, a pointer to a Gdrive_Transfer struct that can be used to
 *      describe and perform a download or upload. On failure, NULL. When no
 *      longer needed, the returned pointer should be passed to 
 *      gdrive_xfer_free().
 * Note:
 *      gdrive_xfer_create() must be called first, and gdrive_xfer_execute()
 *      followed by gdrive_xfer_free() must be called last. Aside from those
 *      restrictions, the other gdrive_xfer* functions can be called in any
 *      order. For example, URL query parameters can be added before setting the
 *      URL with no adverse effects.
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

/*
 * gdrive_xfer_set_requesttype():   Set the HTTP Request type for a transfer.
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):   
 *              Pointer to a Gdrive_Transfer struct created by an earlier call 
 *              to gdrive_xfer_create().
 *      requestType (enum Gdrive_Request_Type): 
 *              The request type. Valid values are:
 *              GDRIVE_REQUEST_GET,
 *              GDRIVE_REQUEST_POST,
 *              GDRIVE_REQUEST_PUT,
 *              GDRIVE_REQUEST_PATCH,
 *              GDRIVE_REQUEST_DELETE
 */
void gdrive_xfer_set_requesttype(Gdrive_Transfer* pTransfer, 
                                 enum Gdrive_Request_Type requestType
);

/*
 * gdrive_xfer_set_retryonautherror():  Determines whether or not to refresh
 *                                      credentials and retry the transfer if
 *                                      authentication fails. This behavior is
 *                                      on by default and only needs set if
 *                                      the default behavior is not desired.
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):   
 *              Pointer to a Gdrive_Transfer struct created by an earlier call 
 *              to gdrive_xfer_create().
 *      retry (bool):   
 *              If true, will refresh credentials and retry the transfer upon 
 *              authentication failure. If false, authentication failure will 
 *              not cause a retry.
 */
void gdrive_xfer_set_retryonautherror(Gdrive_Transfer* pTransfer, bool retry);

/*
 * gdrive_xfer_set_url():   Set the URL for a transfer. This is mandatory for
 *                          every transfer.
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):   
 *              Pointer to a Gdrive_Transfer struct created by an earlier call 
 *              to gdrive_xfer_create().
 *      url (const char*):  
 *              A null-terminated string containing the base URL. Although query
 *              parameters can be included here, it is better practice to leave 
 *              query parameters (and the '?' separator) out. Query parameters 
 *              should be added with gdrive_xfer_add_query().
 * Return value (int):
 *      0 on success, non-zero on failure.
 */
int gdrive_xfer_set_url(Gdrive_Transfer* pTransfer, const char* url);

/*
 * gdrive_xfer_set_destfile():  Sets the download destination to an open file
 *                              stream. This is optional and only needs done
 *                              if downloading to a memory buffer is not 
 *                              desired.
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):   
 *              Pointer to a Gdrive_Transfer struct created by an earlier call 
 *              to gdrive_xfer_create()..
 *      destFile (FILE*):   
 *              A file stream that is already open for writing.
 */
void gdrive_xfer_set_destfile(Gdrive_Transfer* pTransfer, FILE* destFile);

/*
 * gdrive_xfer_set_body():  Set the body of the HTTP request explicitly. Only
 *                          one of gdrive_xfer_set_body(),
 *                          gdrive_xfer_set_uploadcallback(), or
 *                          gdrive_xfer_add_postfield() should be used for any
 *                          one transfer.
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):   
 *              Pointer to a Gdrive_Transfer struct created by an earlier call 
 *              to gdrive_xfer_create().
 *      body (const char*): 
 *              A null-terminated string containing the text to use as the 
 *              request body. This string is NOT copied, so it must live as long
 *              as pTransfer does.
 */
void gdrive_xfer_set_body(Gdrive_Transfer* pTransfer, const char* body);

/*
 * gdrive_xfer_set_uploadcallback():    Set a callback function to supply the
 *                                      request body for a transfer. Only one of
 *                                      gdrive_xfer_set_body(),
 *                                      gdrive_xfer_set_uploadcallback(), or
 *                                      gdrive_xfer_add_postfield() should be 
 *                                      used for any one transfer.
 * 
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):   
 *              Pointer to a Gdrive_Transfer struct created by an earlier call 
 *              to gdrive_xfer_create().
 *      callback (gdrive_xfer_upload_callback): 
 *              A pointer to a callback function. This function will be 
 *              repeatedly called, capturing part of the request body in each
 *              call.
 *      userdata (void*):   
 *              Any state or other information that will be needed by the 
 *              callback function. This parameter will be passed unchanged to 
 *              the callback function.
 */
void gdrive_xfer_set_uploadcallback(Gdrive_Transfer* pTransfer, 
                                    gdrive_xfer_upload_callback callback,
                                    void* userdata
);



/*************************************************************************
 * Other accessible functions
 *************************************************************************/

/*
 * gdrive_xfer_add_query(): Add a single URL query parameter. This function can
 *                          be called multiple times to add additional query
 *                          parameters.
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):   
 *              Pointer to a Gdrive_Transfer struct created by an earlier call 
 *              to gdrive_xfer_create().
 *      field (const char*):    
 *              A null-terminated string containing the name of the parameter. 
 *              An internal copy of the string will be kept, and this internal 
 *              copy will be URL-encoded.
 *      value (const char*):    
 *              A null-terminated string containing the value of the parameter. 
 *              An internal copy of the string will be kept, and this internal 
 *              copy will be URL-encoded.
 * Return value:
 *      0 on success, non-zero on failure.
 */
int gdrive_xfer_add_query(Gdrive_Transfer* pTransfer, 
                          const char* field, 
                          const char* value
);

/*
 * gdrive_xfer_add_postfield(): Add a single field=value pair to the body of an 
 *                              HTTP POST request. This function can be called 
 *                              multiple times to add additional data.
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):   
 *              Pointer to a Gdrive_Transfer struct created by an earlier call 
 *              to gdrive_xfer_create().
 *      field (const char*):    
 *              A null-terminated string containing the name of the parameter. 
 *              An internal copy of the string will be kept, and this internal 
 *              copy will be URL-encoded.
 *      value (const char*):    
 *              A null-terminated string containing the value of the parameter. 
 *              An internal copy of the string will be kept, and this internal 
 *              copy will be URL-encoded.
 * Return value:
 *      0 on success, non-zero on failure.
 */
int gdrive_xfer_add_postfield(Gdrive_Transfer* pTransfer, 
                          const char* field, 
                          const char* value
);

/*
 * gdrive_xfer_add_header():    Add a single HTTP header, or remove a header 
 *                              that libcurl includes by default. This function
 *                              can be called multiple times.
 * Parameters:
 *      pTransfer (Gdrive_Transfer*):   
 *              Pointer to a Gdrive_Transfer struct created by an earlier call 
 *              to gdrive_xfer_create().
 *      header (const char*):   
 *              A null-terminated string containing a header. This can be any of
 *              the following forms:
 *              * "name: value" to add a new header with a value or to override 
 *                  the value of a libcurl default,
 *              * "name:" to remove a libcurl default header, or
 *              * "name;" to add a new header without a value or to remove the 
 *                  value of a libcurl default (without removing the header 
 *                  itself).
 * Notes:
 *      Some headers, such as "Content-Type" are included by libcurl by default.
 *      In addition, if we already have a Google Drive access token, then an
 *      "Authorization" header will be included, with the value 
 *      "Bearer <token>". There is no need to add the "Authorization" header.
 */
int gdrive_xfer_add_header(Gdrive_Transfer* pTransfer, const char* header);

/*
 * gdrive_xfer_execute():   Perform the upload or download operation described
 *                          by a Gdrive_Transfer struct. If the transfer results
 *                          in an HTTP status code of 5XX (server error) or in
 *                          a Rate Limit Exceeded error, it will be retried 
 *                          using an exponential backoff strategy, up to a 
 *                          maximum of GDRIVE_RETRY_LIMIT attempts. Unless
 *                          gdrive_xfer_set_retryonautherror() has been called
 *                          with a value of false, authentication errors are
 *                          also retried after refreshing authentication 
 *                          information.
 * Return value (Gdrive_Download_Buffer*):
 *      A pointer to a Gdrive_Download_Buffer struct containing the results of
 *      the transfer. The caller is responsible for passing the returned pointer
 *      to gdrive_dlbuf_free().
 */
Gdrive_Download_Buffer* gdrive_xfer_execute(Gdrive_Transfer* pTransfer);





#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_TRANSFER_DESCRIPTOR_H */

