/* 
 * File:   fuse-drive.c
 * Author: me
 *
 * Created on December 28, 2014, 11:00 AM
 */

#ifndef __GDRIVE_TEST__

#define _XOPEN_SOURCE 500


// Library and standard headers
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>


// Temporary includes for testing
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>

// Project header(s)
#include "fuse-drive.h"
#include "gdrive.h"




/**
 * DOCUMENTATION FROM fuse.h:
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 * 
 * -------
 * FROM man PAGE FOR access(2):
 * access() shall fail if:
 *
 *      EACCES The requested access would be denied to the file, or search per‐
 *             mission is denied for one of the directories in the path  prefix
 *             of pathname.  (See also path_resolution(7).)
 *
 *      ELOOP  Too many symbolic links were encountered in resolving pathname.
 *
 *      ENAMETOOLONG
 *             pathname is too long.
 *
 *      ENOENT A component of pathname does not exist or is a dangling symbolic
 *             link.
 *
 *      ENOTDIR
 *             A component used as a directory in pathname is not, in  fact,  a
 *             directory.
 *
 *      EROFS  Write  permission  was  requested  for  a  file  on  a read-only
 *             filesystem.
 * access() may fail if:
 *
 *      EFAULT pathname points outside your accessible address space.
 *
 *      EINVAL mode was incorrectly specified.
 *
 *      EIO    An I/O error occurred.
 *
 *      ENOMEM Insufficient kernel memory was available.
 *
 *      ETXTBSY
 *             Write access was requested to an executable which is being  exe‐
 *             cuted.
 *
 */
static int fudr_access(const char* path, int mask)
{
    //Either just have a default set of permissions, or have a default plus 
    //store modified permissions as private file metadata.
    return -ENOSYS;
}


//bmap only makes sense for block devices.
//static int fudr_bmap(const char* path, size_t blocksize, uint64_t* blockno)
//{
//    
//}

/*
 * FROM man PAGE FOR chmod(2):
 * The  new  file  permissions  are specified in mode, which is a bit mask
 *     created by ORing together zero or more of the following:
 *
 *     S_ISUID  (04000)  set-user-ID  (set  process  effective  user   ID   on
 *                       execve(2))
 *
 *     S_ISGID  (02000)  set-group-ID  (set  process  effective  group  ID  on
 *                       execve(2);  mandatory  locking,   as   described   in
 *                       fcntl(2);  take a new file's group from parent direc‐
 *                       tory, as described in chown(2) and mkdir(2))
 *
 *     S_ISVTX  (01000)  sticky bit (restricted deletion flag, as described in
 *                       unlink(2))
 *
 *     S_IRUSR  (00400)  read by owner
 *
 *     S_IWUSR  (00200)  write by owner
 *
 *     S_IXUSR  (00100)  execute/search  by owner ("search" applies for direc‐
 *                   tories, and means that entries within  the  directory
 *                       can be accessed)
 *
 *     S_IRGRP  (00040)  read by group
 *
 *     S_IWGRP  (00020)  write by group
 *
 *     S_IXGRP  (00010)  execute/search by group
 *
 *     S_IROTH  (00004)  read by others
 *
 *     S_IWOTH  (00002)  write by others
 *
 *     S_IXOTH  (00001)  execute/search by others
 *
 *     The  effective  UID  of the calling process must match the owner of the
 *     file, or the process must  be  privileged  (Linux:  it  must  have  the
 *     CAP_FOWNER capability).
 * ----
 * Depending on the filesystem, other errors can be  returned.   The  more
 *     general errors for chmod() are listed below:
 *
 *     EACCES Search  permission  is denied on a component of the path prefix.
 *            (See also path_resolution(7).)
 *
 *     EFAULT path points outside your accessible address space.
 *
 *     EIO    An I/O error occurred.
 *
 *     ELOOP  Too many symbolic links were encountered in resolving path.
 *
 *     ENAMETOOLONG
 *            path is too long.
 *
 *     ENOENT The file does not exist.
 *
 *     ENOMEM Insufficient kernel memory was available.
 *
 *     ENOTDIR
 *            A component of the path prefix is not a directory.
 *     
 *      EPERM  The effective UID does not match the owner of the file, and  the
 *            process   is  not  privileged  (Linux:  it  does  not  have  the
 *            CAP_FOWNER capability).
 *
 *     EROFS  The named file resides on a read-only filesystem.
 */
static int fudr_chmod(const char* path, mode_t mode)
{
    //If this is implemented at all, will need to store new permissions as
    //private file metadata.  Should check the owners[] list in file metadata,
    //return -EPERM unless IsAuthenticatedUser is true for one of the owners.
    return -ENOSYS;
}

//chown probably does not make sense for our needs.
static int fudr_chown(const char* path, uid_t uid, gid_t gid)
{
    return -ENOSYS;
}

/**
 *  DOCUMENTATION FROM fuse.h:
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 * 
 * -----------
 * FROM man PAGE FOR create(2):
 * No manual entry for create
 */
static int fudr_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
    return -ENOSYS;
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
    gdrive_cleanup((Gdrive_Info*) private_data);
}

/**
 * DOCUMENTATION FROM fuse.h:
 * Allocates space for an open file
 *
 * This function ensures that required space is allocated for specified
 * file.  If this function returns success then any subsequent write
 * request to specified range is guaranteed not to fail because of lack
 * of space on the file system media.
 *
 * Introduced in version 2.9.1
 */
static int fudr_fallocate(const char* path, int mode, off_t offset, 
        off_t len, struct fuse_file_info* fi)
{
    return -ENOSYS;
}
static int fudr_fgetattr(const char* path, struct stat* stbuf, 
        struct fuse_file_info* fi)
{
    return -ENOSYS;
}

//According to fuse.h, the kernel handles local file locking if flock is
//not implemented.  Files probably can't be locked on Google Drive, so 
//don't implement this.
//static int fudr_flock(const char* path, struct fuse_file_info* fi, int op)
//{
//    
//}

static int fudr_flush(const char* path, struct fuse_file_info* fi)
{
    return -ENOSYS;
}
static int fudr_fsync(const char* path, int isdatasync, 
        struct fuse_file_info* fi)
{
    return -ENOSYS;
}
static int fudr_fsyncdir(const char* path, int isdatasync, struct 
fuse_file_info* fi)
{
    return -ENOSYS;
}
static int fudr_ftruncate(const char* path, off_t size, struct 
fuse_file_info* fi)
{
    return -ENOSYS;
}

static int fudr_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));
    
    Gdrive_Info* pGdriveInfo = (Gdrive_Info*) fuse_get_context()->private_data;
    char* fileId = gdrive_filepath_to_id(pGdriveInfo, path);
    if (fileId == NULL)
    {
        // File not found
        return -ENOENT;
    }
    
    Gdrive_Fileinfo fileinfo;
    memset(&fileinfo, 0, sizeof(fileinfo));
    if (gdrive_file_info_from_id(pGdriveInfo, fileId, &fileinfo) != 0)
    {
        // An error occurred.
        free(fileId);
        return -ENOMEM;
    }
    
    switch(fileinfo.type)
    {
    case GDRIVE_FILETYPE_FOLDER:
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = fileinfo.nParents + 1; // Add 1 for "."
        break;
        
    case GDRIVE_FILETYPE_FILE:
    default:
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = fileinfo.nParents;
    }
    
    stbuf->st_uid = geteuid();
    stbuf->st_gid = getegid();
    stbuf->st_size = fileinfo.size;
    stbuf->st_atime = fileinfo.accessTime;
    stbuf->st_mtime = fileinfo.modificationTime;
    stbuf->st_ctime = fileinfo.creationTime;
    
    free(fileId);
    return 0;
}

static int fudr_getxattr(const char* path, const char* name, char* value, size_t size)
{
    return -ENOSYS;
}
static void* fudr_init(struct fuse_conn_info *conn)
{
    return NULL;
}
static int fudr_ioctl(const char* path, int cmd, void* arg, 
        struct fuse_file_info* fi, unsigned int flags, void* data)
{
    return -ENOSYS;
}
static int fudr_link(const char* from, const char* to)
{
    return -ENOSYS;
}
static int fudr_listxattr(const char* path, char* list, size_t size)
{
    return -ENOSYS;
}
static int fudr_lock(const char* path, struct fuse_file_info* fi, int cmd, struct flock* locks)
{
    return -ENOSYS;
}
static int fudr_mkdir(const char* path, mode_t mode)
{
    return -ENOSYS;
}
static int fudr_mknod(const char* path, mode_t mode, dev_t rdev)
{
    return -ENOSYS;
}


static int fudr_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, "/Test") != 0)
        return -ENOENT;
    if ((fi->flags & 3) != O_RDONLY)
        return -EACCES;
    return 0;
}

static int fudr_opendir(const char* path, struct fuse_file_info* fi)
{
    return -ENOSYS;
}
static int fudr_poll(const char* path, struct fuse_file_info* fi, 
        struct fuse_pollhandle* ph, unsigned* reventsp)
{
    return -ENOSYS;
}

static int fudr_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
    struct fuse_context *context;
    struct passwd *pwd;
    
    char *text; 
    size_t len;
    (void) fi;
    if (strcmp(path, "/Test") != 0)
        return -ENOENT;
    
    context = fuse_get_context();
    pwd = getpwuid(context->uid);
    text = (pwd == NULL) ? "" : pwd->pw_name;
    
    len = strlen(text);
    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, text + offset, size);
    } else
        size = 0;
    
    return size;
}

static int fudr_read_buf(const char* path, struct fuse_bufvec **bufp, 
        size_t size, off_t off, struct fuse_file_info* fi)
{
    return -ENOSYS;
}

static int fudr_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
    // Suppress warnings for unused function parameters
    (void) offset;
    (void) fi;
    
    Gdrive_Info* pGdriveInfo = (Gdrive_Info*) fuse_get_context()->private_data;
    
    char* folderId = gdrive_filepath_to_id(pGdriveInfo, path);
    if (folderId == NULL)
    {
        return -ENOENT;
    }
    
    Gdrive_Fileinfo_Array* pFileArray = gdrive_fileinfo_array_create();
    if (pFileArray == NULL)
    {
        free(folderId);
        return -ENOMEM;
    }
    if (gdrive_folder_list(pGdriveInfo, folderId, pFileArray) == -1)
    {
        // An error occurred.
        free(folderId);
        gdrive_fileinfo_array_free(pFileArray);
        return -ENOENT;
    }
    
    //filler(buf, ".", NULL, 0);
    //filler(buf, "..", NULL, 0);
    for (int i = 0; i < pFileArray->nItems; i++)
    {
        Gdrive_Fileinfo* pCurrentFile = pFileArray->pArray + i;
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
    
    free(folderId);
    gdrive_fileinfo_array_free(pFileArray);
    
    return 0;
}

static int fudr_readlink(const char* path, char* buf, size_t size)
{
    return -ENOSYS;
}

static int fudr_release(const char* path, struct fuse_file_info *fi)
{
    return -ENOSYS;
}
static int fudr_releasedir(const char* path, struct fuse_file_info *fi)
{
    return -ENOSYS;
}
static int fudr_removexattr(const char* path, const char* value)
{
    return -ENOSYS;
}
static int fudr_rename(const char* from, const char* to)
{
    return -ENOSYS;
}
static int fudr_rmdir(const char* path)
{
    return -ENOSYS;
}
static int fudr_setxattr(const char* path, const char* name, 
        const char* value, size_t size, int flags)
{
    return -ENOSYS;
}
static int fudr_statfs(const char* path, struct statvfs* stbuf)
{
    return -ENOSYS;
}
static int fudr_symlink(const char* to, const char* from)
{
    return -ENOSYS;
}
static int fudr_truncate(const char* path, off_t size)
{
    return -ENOSYS;
}
static int fudr_unlink(const char* path)
{
    return -ENOSYS;
}
static int fudr_utime()
{
    return -ENOSYS;
}
static int fudr_utimens(const char* path, const struct timespec ts[2])
{
    return -ENOSYS;
}
static int fudr_write(const char* path, const char *buf, size_t size, 
        off_t offset, struct fuse_file_info* fi)
{
    return -ENOSYS;
}
static int fudr_write_buf(const char* path, struct fuse_bufvec* buf, 
        off_t off, struct fuse_file_info* fi)
{
    return -ENOSYS;
}


static struct fuse_operations fo = {
    .access         = NULL, //fudr_access,
    .bmap           = NULL, //fudr_bmap,
    .chmod          = NULL, //fudr_chmod,
    .chown          = NULL, //fudr_chown,
    .create         = NULL, //fudr_create,
    .destroy        = fudr_destroy,
    .fallocate      = NULL, //fudr_fallocate,
    .fgetattr       = NULL, //fudr_fgetattr,
    .flock          = NULL, //fudr_flock,
    .flush          = NULL, //fudr_flush,
    .fsync          = NULL, //fudr_fsync,
    .fsyncdir       = NULL, //fudr_fsyncdir,
    .ftruncate      = NULL, //fudr_ftruncate,
    .getattr        = fudr_getattr,
    .getxattr       = NULL, //fudr_getxattr,
    .init           = NULL, //fudr_init,
    .ioctl          = NULL, //fudr_ioctl,
    .link           = NULL, //fudr_link,
    .listxattr      = NULL, //fudr_listxattr,
    .lock           = NULL, //fudr_lock,
    .mkdir          = NULL, //fudr_mkdir,
    .mknod          = NULL, //fudr_mknod,
    .open           = fudr_open,
    .opendir        = NULL, //fudr_opendir,
    .poll           = NULL, //fudr_poll,
    .read           = fudr_read,
    .read_buf       = NULL, //fudr_read_buf,
    .readdir        = fudr_readdir,
    .readlink       = NULL, //fudr_readlink,
    .release        = NULL, //fudr_release,
    .releasedir     = NULL, //fudr_releasedir,
    .removexattr    = NULL, //fudr_removexattr,
    .rename         = NULL, //fudr_rename,
    .rmdir          = NULL, //fudr_rmdir,
    .setxattr       = NULL, //fudr_setxattr,
    .statfs         = NULL, //fudr_statfs,
    .symlink        = NULL, //fudr_symlink,
    .truncate       = NULL, //fudr_truncate,
    .unlink         = NULL, //fudr_unlink,
    .utime          = NULL, //fudr_utime,
    .utimens        = NULL, //fudr_utimens,
    .write          = NULL, //fudr_write,
    .write_buf      = NULL, //fudr_write_buf,
};


/*
 * 
 */
int main(int argc, char** argv) 
{
    Gdrive_Info* pGdrive = NULL;
    if ((gdrive_init(&pGdrive, GDRIVE_ACCESS_META, "/home/me/.fuse-drive/.auth", GDRIVE_INTERACTION_STARTUP)) != 0)
    {
        printf("Could not set up a Google Drive connection.");
        return 1;
    }
    return fuse_main(argc, argv, &fo, pGdrive);
}

#endif	/*__GDRIVE_TEST__*/