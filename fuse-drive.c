/* 
 * File:   fuse-drive.c
 * Author: me
 *
 * 
 */

#ifndef __GDRIVE_TEST__

#define _XOPEN_SOURCE 500


// Library and standard headers
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>



// Project header(s)
#include "fuse-drive.h"
#include "gdrive/gdrive.h"


int fudr_stat_from_fileinfo(const Gdrive_Fileinfo* pFileinfo, 
                            bool isRoot, 
                            struct stat* stbuf
)
{
    switch(pFileinfo->type)
    {
    case GDRIVE_FILETYPE_FOLDER:
        stbuf->st_mode = S_IFDIR;
        stbuf->st_nlink = pFileinfo->nParents + pFileinfo->nChildren;
        // Account for ".".  Also, if the root of the filesystem, account for 
        // "..", which is outside of the Google Drive filesystem and thus not
        // included in nParents.
        stbuf->st_nlink += isRoot ? 2 : 1;
        break;
        
    case GDRIVE_FILETYPE_FILE:
    default:
        stbuf->st_mode = S_IFREG;
        stbuf->st_nlink = pFileinfo->nParents;
    }
    
    int perms = gdrive_finfo_real_perms(pFileinfo);
    // For now, Owner/Group/User permissions are identical.  Eventually should
    // add a command-line option for a umask or similar.
    // Owner permissions.
    stbuf->st_mode = stbuf->st_mode | (perms << 6);
    // Group permissions
    stbuf->st_mode = stbuf->st_mode | (perms << 3);
    // User permissions
    stbuf->st_mode = stbuf->st_mode | (perms);
    
    stbuf->st_uid = geteuid();
    stbuf->st_gid = getegid();
    stbuf->st_size = pFileinfo->size;
    stbuf->st_atime = pFileinfo->accessTime.tv_sec;
    stbuf->st_mtime = pFileinfo->modificationTime.tv_sec;
    stbuf->st_ctime = pFileinfo->creationTime.tv_sec;
    
    return 0;
}




///**
// * DOCUMENTATION FROM fuse.h:
// * Check file access permissions
// *
// * This will be called for the access() system call.  If the
// * 'default_permissions' mount option is given, this method is not
// * called.
// *
// * This method is not called under Linux kernel versions 2.4.x
// *
// * Introduced in version 2.5
// * 
// * -------
// * FROM man PAGE FOR access(2):
// * access() shall fail if:
// *
// *      EACCES The requested access would be denied to the file, or search per‐
// *             mission is denied for one of the directories in the path  prefix
// *             of pathname.  (See also path_resolution(7).)
// *
// *      ELOOP  Too many symbolic links were encountered in resolving pathname.
// *
// *      ENAMETOOLONG
// *             pathname is too long.
// *
// *      ENOENT A component of pathname does not exist or is a dangling symbolic
// *             link.
// *
// *      ENOTDIR
// *             A component used as a directory in pathname is not, in  fact,  a
// *             directory.
// *
// *      EROFS  Write  permission  was  requested  for  a  file  on  a read-only
// *             filesystem.
// * access() may fail if:
// *
// *      EFAULT pathname points outside your accessible address space.
// *
// *      EINVAL mode was incorrectly specified.
// *
// *      EIO    An I/O error occurred.
// *
// *      ENOMEM Insufficient kernel memory was available.
// *
// *      ETXTBSY
// *             Write access was requested to an executable which is being  exe‐
// *             cuted.
// *
// */
//static int fudr_access(const char* path, int mask)
//{
//    //Either just have a default set of permissions, or have a default plus 
//    //store modified permissions as private file metadata.
//    return -ENOSYS;
//}
//
//
////bmap only makes sense for block devices.
////static int fudr_bmap(const char* path, size_t blocksize, uint64_t* blockno)
////{
////    
////}
//
///*
// * FROM man PAGE FOR chmod(2):
// * The  new  file  permissions  are specified in mode, which is a bit mask
// *     created by ORing together zero or more of the following:
// *
// *     S_ISUID  (04000)  set-user-ID  (set  process  effective  user   ID   on
// *                       execve(2))
// *
// *     S_ISGID  (02000)  set-group-ID  (set  process  effective  group  ID  on
// *                       execve(2);  mandatory  locking,   as   described   in
// *                       fcntl(2);  take a new file's group from parent direc‐
// *                       tory, as described in chown(2) and mkdir(2))
// *
// *     S_ISVTX  (01000)  sticky bit (restricted deletion flag, as described in
// *                       unlink(2))
// *
// *     S_IRUSR  (00400)  read by owner
// *
// *     S_IWUSR  (00200)  write by owner
// *
// *     S_IXUSR  (00100)  execute/search  by owner ("search" applies for direc‐
// *                   tories, and means that entries within  the  directory
// *                       can be accessed)
// *
// *     S_IRGRP  (00040)  read by group
// *
// *     S_IWGRP  (00020)  write by group
// *
// *     S_IXGRP  (00010)  execute/search by group
// *
// *     S_IROTH  (00004)  read by others
// *
// *     S_IWOTH  (00002)  write by others
// *
// *     S_IXOTH  (00001)  execute/search by others
// *
// *     The  effective  UID  of the calling process must match the owner of the
// *     file, or the process must  be  privileged  (Linux:  it  must  have  the
// *     CAP_FOWNER capability).
// * ----
// * Depending on the filesystem, other errors can be  returned.   The  more
// *     general errors for chmod() are listed below:
// *
// *     EACCES Search  permission  is denied on a component of the path prefix.
// *            (See also path_resolution(7).)
// *
// *     EFAULT path points outside your accessible address space.
// *
// *     EIO    An I/O error occurred.
// *
// *     ELOOP  Too many symbolic links were encountered in resolving path.
// *
// *     ENAMETOOLONG
// *            path is too long.
// *
// *     ENOENT The file does not exist.
// *
// *     ENOMEM Insufficient kernel memory was available.
// *
// *     ENOTDIR
// *            A component of the path prefix is not a directory.
// *     
// *      EPERM  The effective UID does not match the owner of the file, and  the
// *            process   is  not  privileged  (Linux:  it  does  not  have  the
// *            CAP_FOWNER capability).
// *
// *     EROFS  The named file resides on a read-only filesystem.
// */
//static int fudr_chmod(const char* path, mode_t mode)
//{
//    //If this is implemented at all, will need to store new permissions as
//    //private file metadata.  Should check the owners[] list in file metadata,
//    //return -EPERM unless IsAuthenticatedUser is true for one of the owners.
//    return -ENOSYS;
//}
//
////chown probably does not make sense for our needs.
//static int fudr_chown(const char* path, uid_t uid, gid_t gid)
//{
//    return -ENOSYS;
//}
//
///**
// *  DOCUMENTATION FROM fuse.h:
// * Create and open a file
// *
// * If the file does not exist, first create it with the specified
// * mode, and then open it.
// *
// * If this method is not implemented or under Linux kernel
// * versions earlier than 2.6.15, the mknod() and open() methods
// * will be called instead.
// *
// * Introduced in version 2.5
// * 
// * -----------
// * FROM man PAGE FOR create(2):
// * No manual entry for create
// */
static int fudr_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
    // Silence compiler warning for unused parameter. If fudr_chmod is 
    // implemented, this line should be removed.
    (void) mode;
    
    // Determine whether the file already exists, 
    if (gdrive_filepath_to_id(path) != NULL)
    {
        return -EEXIST;
    }
    
    // Make new copies of path because dirname() and some versions of basename()
    // may modify the arguments
    size_t pathSize = strlen(path) + 1;
    char* pathDirCopy = malloc(pathSize);
    if (pathDirCopy == NULL)
    {
        // Memory error
        return -ENOMEM;
    }
    char* pathNameCopy = malloc(pathSize);
    if (pathNameCopy == NULL)
    {
        // Memory error
        return -ENOMEM;
    }
    memcpy(pathDirCopy, path, pathSize);
    memcpy(pathNameCopy, path, pathSize);
    
    // Find the parent folder without the base filename, and check for the 
    // folder's validity
    const char* folderName = dirname(pathDirCopy);
    if (folderName == NULL || folderName[0] != '/')
    {
        // Invalid folder
        free(pathDirCopy);
        free(pathNameCopy);
        return -ENOTDIR;
    }
    
    // Get the base filename and check for validity
    const char* filename = basename(pathNameCopy);
    if (filename == NULL || filename[0] == '/' || 
            strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
    {
        // Invalid filename
        free(pathDirCopy);
        free(pathNameCopy);
        return -EISDIR;
    }
    
    const char* folderId = gdrive_filepath_to_id(folderName);
    if (folderId == NULL)
    {
        // Folder doesn't exist
        free(pathDirCopy);
        free(pathNameCopy);
        return -ENOTDIR;
    }
    
    // Create the file
    int error = 0;
    const char* fileId = gdrive_file_new(folderId, path, filename, &error);
    if (fileId == NULL)
    {
        // Some error occurred
        free(pathDirCopy);
        free(pathNameCopy);
        return -error;
    }
    
    // TODO: If fudr_chmod is ever implemented, change the file permissions 
    // using the (currently unused) mode parameter we were given.
    
    // File was successfully created. Open it.
    //int error = 0;
    fi->fh = (uint64_t) gdrive_file_open(fileId, O_RDWR, &error);
    
    
    free(pathDirCopy);
    free(pathNameCopy);
    
    return -error;
}

/**
 * DOCUMENTATION FROM fuse.h:
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
static void fudr_destroy(void* private_data)
{
    // Silence compiler warning about unused parameter
    (void) private_data;
    
    gdrive_cleanup();
}

///**
// * DOCUMENTATION FROM fuse.h:
// * Allocates space for an open file
// *
// * This function ensures that required space is allocated for specified
// * file.  If this function returns success then any subsequent write
// * request to specified range is guaranteed not to fail because of lack
// * of space on the file system media.
// *
// * Introduced in version 2.9.1
// */
//static int fudr_fallocate(const char* path, int mode, off_t offset, 
//        off_t len, struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}
static int fudr_fgetattr(const char* path, struct stat* stbuf, 
        struct fuse_file_info* fi)
{
    Gdrive_File* fh = (Gdrive_File*) fi->fh;
    const Gdrive_Fileinfo* pFileinfo = gdrive_file_get_info(fh);
    
    if (pFileinfo == NULL)
    {
        // Invalid file handle
        return -EBADF;
    }
    
    int returnVal = fudr_stat_from_fileinfo(pFileinfo, strcmp(path, "/") == 0, stbuf);
    return returnVal;
    
}
//
////According to fuse.h, the kernel handles local file locking if flock is
////not implemented.  Files probably can't be locked on Google Drive, so 
////don't implement this.
////static int fudr_flock(const char* path, struct fuse_file_info* fi, int op)
////{
////    
////}
//
//static int fudr_flush(const char* path, struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}
static int fudr_fsync(const char* path, int isdatasync, 
        struct fuse_file_info* fi)
{
    // Distinguishing between data-only and data-and-metadata syncs doesn't
    // really help us, so ignore isdatasync. Ignore path since we should have
    // a Gdrive file handle.
    (void) isdatasync;
    (void) path;
    
    return gdrive_file_sync((Gdrive_File*) fi->fh);
}
//static int fudr_fsyncdir(const char* path, int isdatasync, struct 
//fuse_file_info* fi)
//{
//    return -ENOSYS;
//}
static int fudr_ftruncate(const char* path, off_t size, struct 
fuse_file_info* fi)
{
    // Suppress unused parameter compiler warnings
    (void) path;
    
    Gdrive_File* fh = (Gdrive_File*) fi->fh;
    
    int returnVal = gdrive_file_truncate(fh, size);
    return returnVal;
}

static int fudr_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));
    
    const char* fileId = gdrive_filepath_to_id(path);
    if (fileId == NULL)
    {
        // File not found
        return -ENOENT;
    }
    
    //Gdrive_Fileinfo* pFileinfo;
    //if (gdrive_file_info_from_id(pGdriveInfo, fileId, &pFileinfo) != 0)
    const Gdrive_Fileinfo* pFileinfo = gdrive_finfo_get_by_id(fileId);
    if (pFileinfo == NULL)
    {
        // An error occurred.
        //free(fileId);
        return -ENOENT;
    }    
    
    int returnVal = fudr_stat_from_fileinfo(pFileinfo, strcmp(path, "/") == 0, stbuf);
    return returnVal;
}

//static int fudr_getxattr(const char* path, const char* name, char* value, size_t size)
//{
//    return -ENOSYS;
//}
static void* fudr_init(struct fuse_conn_info *conn)
{
    // Add any desired capabilities.
    conn->want = conn->want | FUSE_CAP_ATOMIC_O_TRUNC | FUSE_CAP_BIG_WRITES | FUSE_CAP_EXPORT_SUPPORT;
    // Remove undesired capabilities.
    conn->want = conn->want & !(FUSE_CAP_ASYNC_READ);
    
    // Need to turn off async read here, too.
    conn->async_read = 0;
    
    return fuse_get_context()->private_data;
}
//static int fudr_ioctl(const char* path, int cmd, void* arg, 
//        struct fuse_file_info* fi, unsigned int flags, void* data)
//{
//    return -ENOSYS;
//}
//static int fudr_link(const char* from, const char* to)
//{
//    return -ENOSYS;
//}
//static int fudr_listxattr(const char* path, char* list, size_t size)
//{
//    return -ENOSYS;
//}
//static int fudr_lock(const char* path, struct fuse_file_info* fi, int cmd, struct flock* locks)
//{
//    return -ENOSYS;
//}
//static int fudr_mkdir(const char* path, mode_t mode)
//{
//    return -ENOSYS;
//}
//static int fudr_mknod(const char* path, mode_t mode, dev_t rdev)
//{
//    return -ENOSYS;
//}
//
//
static int fudr_open(const char *path, struct fuse_file_info *fi)
{
    // Get the file ID
    const char* fileId = gdrive_filepath_to_id(path);
    if (fileId == NULL)
    {
        // File not found
        return -ENOENT;
    }
    
    // Open the file
    int error = 0;
    Gdrive_File* pFile = gdrive_file_open(fileId, fi->flags, &error);
    //free(fileId);
    
    if (pFile == NULL)
    {
        // An error occurred.
        return -error;
    }
    
    // Store the file handle
    fi->fh = (uint64_t) pFile;
    return 0;
}
//
//static int fudr_opendir(const char* path, struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}
//static int fudr_poll(const char* path, struct fuse_file_info* fi, 
//        struct fuse_pollhandle* ph, unsigned* reventsp)
//{
//    return -ENOSYS;
//}

static int fudr_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
    // Silence compiler warning about unused parameter
    (void) path;
    
    Gdrive_File* pFile = (Gdrive_File*) fi->fh;
    
    int returnVal = gdrive_file_read(pFile, buf, size, offset);
    return returnVal;
}

//static int fudr_read_buf(const char* path, struct fuse_bufvec **bufp, 
//        size_t size, off_t off, struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}

static int fudr_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
    // Suppress warnings for unused function parameters
    (void) offset;
    (void) fi;
    
    const char* folderId = gdrive_filepath_to_id(path);
    if (folderId == NULL)
    {
        return -ENOENT;
    }
    
    Gdrive_Fileinfo_Array* pFileArray = 
            gdrive_folder_list(folderId);
    if (pFileArray == NULL)
    {
        // An error occurred.
        return -ENOENT;
    }
    
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    const Gdrive_Fileinfo* pCurrentFile;
    for (pCurrentFile = gdrive_finfoarray_get_first(pFileArray); 
            pCurrentFile != NULL; 
            pCurrentFile = gdrive_finfoarray_get_next(pFileArray, pCurrentFile)
            )
    {
        struct stat st = {0};
        switch (pCurrentFile->type)
        {
        case GDRIVE_FILETYPE_FILE:
            st.st_mode = S_IFREG;
            break;
            
        case GDRIVE_FILETYPE_FOLDER:
            st.st_mode = S_IFDIR;
            break;
        }
        filler(buf, pCurrentFile->filename, &st, 0);
    }
    
    gdrive_finfoarray_free(pFileArray);
    
    return 0;
}

//static int fudr_readlink(const char* path, char* buf, size_t size)
//{
//    return -ENOSYS;
//}
//
static int fudr_release(const char* path, struct fuse_file_info *fi)
{
    // Suppress unused parameter warning
    (void) path;
    
    gdrive_file_close((Gdrive_File*)fi->fh, fi->flags);
    return 0;
}
//static int fudr_releasedir(const char* path, struct fuse_file_info *fi)
//{
//    return -ENOSYS;
//}
//static int fudr_removexattr(const char* path, const char* value)
//{
//    return -ENOSYS;
//}
//static int fudr_rename(const char* from, const char* to)
//{
//    return -ENOSYS;
//}
//static int fudr_rmdir(const char* path)
//{
//    return -ENOSYS;
//}
//static int fudr_setxattr(const char* path, const char* name, 
//        const char* value, size_t size, int flags)
//{
//    return -ENOSYS;
//}
static int fudr_statfs(const char* path, struct statvfs* stbuf)
{
    // Suppress compiler warning about unused parameter
    (void) path;
    
    unsigned long blockSize = gdrive_get_minchunksize();
    unsigned long bytesTotal = gdrive_sysinfo_get_size();
    unsigned long bytesFree = bytesTotal - gdrive_sysinfo_get_used();
    
    memset(stbuf, 0, sizeof(statvfs));
    stbuf->f_bsize = blockSize;
    stbuf->f_blocks = bytesTotal / blockSize;
    stbuf->f_bfree = bytesFree / blockSize;
    stbuf->f_bavail = stbuf->f_bfree;
    
    return 0;
}
//static int fudr_symlink(const char* to, const char* from)
//{
//    return -ENOSYS;
//}
static int fudr_truncate(const char* path, off_t size)
{
    const char* fileId = gdrive_filepath_to_id(path);
    if (fileId == NULL)
    {
        // File not found
        return -ENOENT;
    }
    
    // Open the file
    int error = 0;
    Gdrive_File* fh = gdrive_file_open(fileId, O_RDWR, &error);
    if (fh == NULL)
    {
        // Error
        return error;
    }
    
    // Truncate
    int result = gdrive_file_truncate(fh, size);
    
    // Close
    gdrive_file_close(fh, O_RDWR);
    
    return result;
}
//static int fudr_unlink(const char* path)
//{
//    return -ENOSYS;
//}
//static int fudr_utime()
//{
//    return -ENOSYS;
//}
//static int fudr_utimens(const char* path, const struct timespec ts[2])
//{
//    return -ENOSYS;
//}
static int fudr_write(const char* path, const char *buf, size_t size, 
        off_t offset, struct fuse_file_info* fi)
{
    // Avoid compiler warning for unused variable
    (void) path;
    
    Gdrive_File* fh = (Gdrive_File*) fi->fh;
    int returnVal = gdrive_file_write(fh, buf, size, offset);
    return returnVal;
}
//static int fudr_write_buf(const char* path, struct fuse_bufvec* buf, 
//        off_t off, struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}


static struct fuse_operations fo = {
    .access         = NULL, //fudr_access,      // Need
    .bmap           = NULL, //fudr_bmap,        // no
    .chmod          = NULL, //fudr_chmod,       // Maybe
    .chown          = NULL, //fudr_chown,       // Maybe
    .create         = fudr_create,
    .destroy        = fudr_destroy,
    .fallocate      = NULL, //fudr_fallocate,   // no
    .fgetattr       = fudr_fgetattr,
    .flock          = NULL, //fudr_flock,       // no
    .flush          = NULL, //fudr_flush,       // no
    .fsync          = fudr_fsync,
    .fsyncdir       = NULL, //fudr_fsyncdir,    // no
    .ftruncate      = fudr_ftruncate,
    .getattr        = fudr_getattr,
    .getxattr       = NULL, //fudr_getxattr,    // no
    .init           = fudr_init,
    .ioctl          = NULL, //fudr_ioctl,       // no
    .link           = NULL, //fudr_link,        // Need
    .listxattr      = NULL, //fudr_listxattr,   // no
    .lock           = NULL, //fudr_lock,        // no
    .mkdir          = NULL, //fudr_mkdir,       // Need
    .mknod          = NULL, //fudr_mknod,       // Maybe
    .open           = fudr_open,
    .opendir        = NULL, //fudr_opendir,     // no
    .poll           = NULL, //fudr_poll,        // no
    .read           = fudr_read,
    .read_buf       = NULL, //fudr_read_buf,    // ???
    .readdir        = fudr_readdir,
    .readlink       = NULL, //fudr_readlink,    // Maybe
    .release        = fudr_release,
    .releasedir     = NULL, //fudr_releasedir,  // no
    .removexattr    = NULL, //fudr_removexattr, // no
    .rename         = NULL, //fudr_rename,      // Need
    .rmdir          = NULL, //fudr_rmdir,       // Need
    .setxattr       = NULL, //fudr_setxattr,    // no
    .statfs         = fudr_statfs,
    .symlink        = NULL, //fudr_symlink,     // Maybe
    .truncate       = fudr_truncate,
    .unlink         = NULL, //fudr_unlink,      // Need
    .utime          = NULL, //fudr_utime,       // no
    .utimens        = NULL, //fudr_utimens,     // Need
    .write          = fudr_write,
    .write_buf      = NULL, //fudr_write_buf,   // ????
};


/*
 * 
 */
int main(int argc, char** argv) 
{
    if ((gdrive_init(GDRIVE_ACCESS_WRITE, "/home/me/.fuse-drive/.auth", 10, GDRIVE_INTERACTION_STARTUP, GDRIVE_BASE_CHUNK_SIZE * 4, 15)) != 0)
    {
        printf("Could not set up a Google Drive connection.");
        return 1;
    }
    return fuse_main(argc, argv, &fo, NULL);
}

#endif	/*__GDRIVE_TEST__*/

