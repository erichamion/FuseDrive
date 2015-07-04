/* 
 * File:   gdrive-download-buffer.h
 * Author: me
 * 
 * This header file should be included by gdrive-internal.h
 * 
 * gdrive-download-buffer: A struct and related functions to manage downloading
 * data into an in-memory buffer or into a file on disk.
 *
 * Created on May 3, 2015, 4:12 PM
 */

#ifndef GDRIVE_DOWNLOAD_BUFFER_H
#define	GDRIVE_DOWNLOAD_BUFFER_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "gdrive.h"
    
#include <curl/curl.h>
    

typedef struct Gdrive_Download_Buffer Gdrive_Download_Buffer;

enum Gdrive_Retry_Method
{
    GDRIVE_RETRY_NORETRY,
    GDRIVE_RETRY_RETRY,
    GDRIVE_RETRY_RENEWAUTH
};

enum Gdrive_Request_Type
{
    GDRIVE_REQUEST_GET,
    GDRIVE_REQUEST_POST,
    GDRIVE_REQUEST_PUT,
    GDRIVE_REQUEST_PATCH,
    GDRIVE_REQUEST_DELETE
};





/*************************************************************************
 * Constructors and destructors
 *************************************************************************/

/*
 * gdrive_dlbuf_create():   Creates a new Gdrive_Download_Buffer struct. Once 
 *                          this struct is no longer needed, the caller should
 *                          call gdrive_dlbuf_free().
 * Parameters:
 *      initialSize (size_t):
 *              The in-memory buffer will be initially allocated with a size
 *              of initialSize bytes. The buffer will grow dynamically as needed
 *              so this is not a limitation on the size of downloaded data. If
 *              a file handle is given in the fh parameter, then initialSize is
 *              recommended to be 0.
 *      fh (FILE*):
 *              If fh is NULL, the downloaded data will be stored in an 
 *              in-memory buffer. Otherwise, it will be written to the stream
 *              specified by this parameter.
 * Return value (Gdrive_Download_Buffer*):
 *      NULL on error, or a pointer to a newly allocated Gdrive_Download_Buffer
 *      struct on success.
 */
Gdrive_Download_Buffer* gdrive_dlbuf_create(size_t initialSize, FILE* fh);

/*
 * gdrive_dlbuf_free(): Frees the memory associated with the struct and any
 *                      in-memory data buffer. If data was written to a FILE*
 *                      stream, does NOT close the stream.
 * Parameters:
 *      pBuf (Gdrive_Download_Buffer*):
 *              A pointer to the struct to be freed. This pointer should no 
 *              longer be used after the function returns.
 */
void gdrive_dlbuf_free(Gdrive_Download_Buffer* pBuf);

/*************************************************************************
 * Getter and setter functions
 *************************************************************************/

/*
 * gdrive_dlbuf_get_httpResp(): Returns the HTTP status code for the transfer.
 * Parameters:
 *      pBuf (Gdrive_Download_Buffer*):
 *              A pointer to the download buffer that performed the transfer.
 * Return value (long):
 *      The HTTP status code for the last transfer done using pBuf. If pBuf has
 *      not completed a transfer, the return value will be 0.
 */
long gdrive_dlbuf_get_httpResp(Gdrive_Download_Buffer* pBuf);

/*
 * gdrive_dlbuf_get_data(): Retrieves the contents of the last download using 
 *                          the specified download buffer, as long as an 
 *                          in-memory buffer was used.
 * Parameters:
 *      pBuf (Gdrive_Download_Buffer*):
 *              A pointer to the download buffer that performed the transfer.
 * Return value (const char*):
 *      The location of the in-memory buffer that holds the contents of the last
 *      download using pBuf. If pBuf has not completed a transfer or used a
 *      FILE* stream, the return value is undefined. Note: The memory pointed
 *      to by this function's return value will be freed by calling 
 *      gdrive_dlbuf_free(pBuf).
 */
const char* gdrive_dlbuf_get_data(Gdrive_Download_Buffer* pBuf);

/*
 * gdrive_dlbuf_get_success():  Returns true if the transfer successfully 
 *                              received a response from the server, false
 *                              otherwise. This does not indicate whether the
 *                              server's response indicates a successful
 *                              transaction (use gdrive_dlbuf_get_httpResp()
 *                              for that), just whether we were able to connect
 *                              and get a response at all.
 * Parameters:
 *      pBuf (Gdrive_Download_Buffer*):
 *              A pointer to the download buffer that performed the transfer.
 * Return value (bool):
 *      True if we successfully connected to the server and received a response,
 *      false if a connection error occurred.
 */
bool gdrive_dlbuf_get_success(Gdrive_Download_Buffer* pBuf);


/*************************************************************************
 * Other accessible functions
 *************************************************************************/

/*
 * gdrive_dlbuf_download(): Perform a transfer and store the result as either
 *                          an in-memory buffer or a FILE* stream.
 * Parameters:
 *      pBuf (Gdrive_Download_Buffer*):
 *              The download buffer to use for storing the results of the 
 *              transfer. If pBuf was created with a FILE* handle, then the 
 *              downloaded data will be written to that stream. Otherwise, it
 *              will be stored in an in-memory buffer, which can be retrieved
 *              with gdrive_dlbuf_get_data().
 * Return value (CURLcode):
 *      Returns the success or failure of the transfer, as reported by curl.
 *      Success is indicated by CURLE_OK. See libcurl documentation for possible
 *      error values.
 */
//CURLcode gdrive_dlbuf_download(Gdrive_Download_Buffer* pBuf);
CURLcode gdrive_dlbuf_download(Gdrive_Download_Buffer* pBuf, CURL* curlHandle);

/*
 * gdrive_dlbuf_download_with_retry():  Perform a download, retrying on any 
 *                                      HTTP 5xx errors or rate limit exceeded
 *                                      errors. Optionally, refreshes auth
 *                                      credentials and retries on 
 *                                      authentication errors.
 * Parameters:
 *      pBuf (Gdrive_Download_Buffer*):
 *              A pointer to a Gdrive_Download_Buffer struct that will be used
 *              to store the results of the download.
 *      retryOnAuthError (bool):
 *              Determines whether to renew authorization and retry on
 *              authentication failure.
 *      tryNum (int):
 *              Reserved for internal use. Must be 0.
 *      maxTries (int):
 *              Maximum number of attempts before finally failing.
 * Return value (int):
 *      0 on success, other on failure.
 */
//int gdrive_dlbuf_download_with_retry(Gdrive_Download_Buffer* pBuf, 
//                                     bool retryOnAuthError,
//                                     int tryNum,
//                                     int maxTries
//);
int gdrive_dlbuf_download_with_retry(Gdrive_Download_Buffer* pBuf, 
                                     CURL* curlHandle,
                                     bool retryOnAuthError,
                                     int tryNum,
                                     int maxTries
);



#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_DOWNLOAD_BUFFER_H */

