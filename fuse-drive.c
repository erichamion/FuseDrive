/* 
 * File:   fuse-drive.c
 * Author: me
 *
 * 
 */

#ifndef __GDRIVE_TEST__



// Library and standard headers
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>
#include <assert.h>



// Project header(s)
#include "fuse-drive.h"
#include "gdrive/gdrive-util.h"
#include "gdrive/gdrive.h"
#include "fuse-drive-options.h"


static int fudr_stat_from_fileinfo(const Gdrive_Fileinfo* pFileinfo, 
                                   bool isRoot, 
                                   struct stat* stbuf
);

static int fudr_rm_file_or_dir_by_id(const char* fileId, const char* parentId);

static unsigned int fudr_get_max_perms(bool isDir);

static bool fudr_group_match(gid_t gidToMatch, gid_t gid, uid_t uid);




static int fudr_access(const char* path, int mask);

//static int fudr_bmap(const char* path, size_t blocksize, uint64_t* blockno);

//static int fudr_chmod(const char* path, mode_t mode);

//static int fudr_chown(const char* path, uid_t uid, gid_t gid);

static int 
fudr_create(const char* path, mode_t mode, struct fuse_file_info* fi);

static void fudr_destroy(void* private_data);

//static int 
//fudr_fallocate(const char* path, int mode, off_t offset, off_t len, 
//               struct fuse_file_info* fi);

static int 
fudr_fgetattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);

//static int fudr_flock(const char* path, struct fuse_file_info* fi, int op);

//static int fudr_flush(const char* path, struct fuse_file_info* fi);

static int 
fudr_fsync(const char* path, int isdatasync, struct fuse_file_info* fi);

//static int 
//fudr_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi);

static int 
fudr_ftruncate(const char* path, off_t size, struct fuse_file_info* fi);

static int fudr_getattr(const char *path, struct stat *stbuf);

//static int 
//fudr_getxattr(const char* path, const char* name, char* value, size_t size);

static void* fudr_init(struct fuse_conn_info *conn);

//static int 
//fudr_ioctl(const char* path, int cmd, void* arg, struct fuse_file_info* fi, 
//           unsigned int flags, void* data);

static int fudr_link(const char* from, const char* to);

//static int fudr_listxattr(const char* path, char* list, size_t size);

//static int 
//fudr_lock(const char* path, struct fuse_file_info* fi, int cmd, 
//          struct flock* locks);

static int fudr_mkdir(const char* path, mode_t mode);

//static int fudr_mknod(const char* path, mode_t mode, dev_t rdev);

static int fudr_open(const char *path, struct fuse_file_info *fi);

//static int fudr_opendir(const char* path, struct fuse_file_info* fi);

//static int 
//fudr_poll(const char* path, struct fuse_file_info* fi, 
//          struct fuse_pollhandle* ph, unsigned* reventsp);

static int 
fudr_read(const char *path, char *buf, size_t size, off_t offset,
          struct fuse_file_info *fi);

//static int 
//fudr_read_buf(const char* path, struct fuse_bufvec **bufp, 
//              size_t size, off_t off, struct fuse_file_info* fi);

static int 
fudr_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi);

//static int fudr_readlink(const char* path, char* buf, size_t size);

static int fudr_release(const char* path, struct fuse_file_info *fi);

//static int fudr_releasedir(const char* path, struct fuse_file_info *fi);

//static int fudr_removexattr(const char* path, const char* value);

static int fudr_rename(const char* from, const char* to);

static int fudr_rmdir(const char* path);

//static int 
//fudr_setxattr(const char* path, const char* name, const char* value, 
//              size_t size, int flags);

static int fudr_statfs(const char* path, struct statvfs* stbuf);

//static int fudr_symlink(const char* to, const char* from);

static int fudr_truncate(const char* path, off_t size);

static int fudr_unlink(const char* path);

//static int fudr_utime();

static int fudr_utimens(const char* path, const struct timespec ts[2]);

static int 
fudr_write(const char* path, const char *buf, size_t size, off_t offset, 
           struct fuse_file_info* fi);

//static int 
//fudr_write_buf(const char* path, struct fuse_bufvec* buf, off_t off, 
//               struct fuse_file_info* fi);




static int fudr_stat_from_fileinfo(const Gdrive_Fileinfo* pFileinfo, 
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
    
    unsigned int perms = gdrive_finfo_real_perms(pFileinfo);
    unsigned int maxPerms = 
        fudr_get_max_perms(pFileinfo->type == GDRIVE_FILETYPE_FOLDER);
    // Owner permissions.
    stbuf->st_mode = stbuf->st_mode | ((perms << 6) & maxPerms);
    // Group permissions
    stbuf->st_mode = stbuf->st_mode | ((perms << 3) & maxPerms);
    // User permissions
    stbuf->st_mode = stbuf->st_mode | ((perms) & maxPerms);
    
    stbuf->st_uid = geteuid();
    stbuf->st_gid = getegid();
    stbuf->st_size = pFileinfo->size;
    stbuf->st_atime = pFileinfo->accessTime.tv_sec;
    stbuf->st_atim.tv_nsec = pFileinfo->accessTime.tv_nsec;
    stbuf->st_mtime = pFileinfo->modificationTime.tv_sec;
    stbuf->st_mtim.tv_nsec = pFileinfo->modificationTime.tv_nsec;
    stbuf->st_ctime = pFileinfo->creationTime.tv_sec;
    stbuf->st_ctim.tv_nsec = pFileinfo->creationTime.tv_nsec;
    
    return 0;
}

static int fudr_rm_file_or_dir_by_id(const char* fileId, const char* parentId)
{
    // The fileId should never be NULL. A NULL parentId is a runtime error, but
    // it shouldn't stop execution. Just check the fileId here.
    assert(fileId != NULL);
    
    // Find the number of parents, which is the number of "hard" links.
    const Gdrive_Fileinfo* pFileinfo = gdrive_finfo_get_by_id(fileId);
    if (pFileinfo == NULL)
    {
        // Error
        return -ENOENT;
    }
    if (pFileinfo->nParents > 1)
    {
        // Multiple "hard" links, just remove the parent
        if (parentId == NULL)
        {
            // Invalid ID for parent folder
            return -ENOENT;
        }
        return gdrive_remove_parent(fileId, parentId);
    }
    // else this is the only hard link. Delete or trash the file.
    
    return gdrive_delete(fileId, parentId);
}

static unsigned int fudr_get_max_perms(bool isDir)
{
    struct fuse_context* context = fuse_get_context();
    unsigned long perms = (unsigned long) context->private_data;
    if (isDir)
        perms >>= 9;
    else
        perms &= 0777;
    
    return perms & ~context->umask;
}

static bool fudr_group_match(gid_t gidToMatch, gid_t gid, uid_t uid)
{
    // Simplest case - primary group matches
    if (gid == gidToMatch)
        return true;
    
    // Get a list of all the users in the group, and see whether the desired
    // user is in the list. It seems like there MUST be a cleaner way to do
    // this!
    const struct passwd* userInfo = getpwuid(uid);
    const struct group* grpInfo = getgrgid(gidToMatch);
    for (char** pName = grpInfo->gr_mem; *pName; pName++)
    {
        if (!strcmp(*pName, userInfo->pw_name))
            return true;
    }
    
    // No match found
    return false;
}




static int fudr_access(const char* path, int mask)
{
    // If fudr_chmod() or fudr_chown() is ever added, this function will likely 
    // need changes.
    
    char* fileId = gdrive_filepath_to_id(path);
    if (!fileId)
    {
        // File doesn't exist
        return -ENOENT;
    }
    const Gdrive_Fileinfo* pFileinfo = gdrive_finfo_get_by_id(fileId);
    free(fileId);
    if (!pFileinfo)
    {
        // Unknown error
        return -EIO;
    }
    
    if (mask == F_OK)
    {
        // Only checking whether the file exists
        return 0;
    }
    
    unsigned int filePerms = gdrive_finfo_real_perms(pFileinfo);
    unsigned int maxPerms = 
        fudr_get_max_perms(pFileinfo->type == GDRIVE_FILETYPE_FOLDER);
    
    const struct fuse_context* context = fuse_get_context();
    
    if (context->uid == geteuid())
    {
        // User permission
        maxPerms >>= 6;
    }
    else if (fudr_group_match(getegid(), context->gid, context->uid))
    {
        // Group permission
        maxPerms >>= 3;
    }
    // else other permission, don't change maxPerms
    
    unsigned int finalPerms = filePerms & maxPerms;
    
    if (((mask & R_OK) && !(finalPerms & S_IROTH)) ||
            ((mask & W_OK) && !(finalPerms & S_IWOTH)) ||
            ((mask & X_OK) && !(finalPerms & S_IXOTH))
            )
        return -EACCES;
    
    return 0;
}

////bmap only makes sense for block devices.
//static int fudr_bmap(const char* path, size_t blocksize, uint64_t* blockno)
//{
//    
//}

//static int fudr_chmod(const char* path, mode_t mode)
//{
//    //If this is implemented at all, will need to store new permissions as
//    //private file metadata.  Should check the owners[] list in file metadata,
//    //return -EPERM unless IsAuthenticatedUser is true for one of the owners.
//    return -ENOSYS;
//}

////chown probably does not make sense for our needs.
//static int fudr_chown(const char* path, uid_t uid, gid_t gid)
//{
//    return -ENOSYS;
//}

static int 
fudr_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
    // Silence compiler warning for unused parameter. If fudr_chmod is 
    // implemented, this line should be removed.
    (void) mode;
    
    // Determine whether the file already exists
    char* dummyFileId = gdrive_filepath_to_id(path);
    free(dummyFileId);
    if (dummyFileId != NULL)
    {
        return -EEXIST;
    }
    
    // Need write access to the parent directory
    Gdrive_Path* pGpath = gdrive_path_create(path);
    if (!pGpath)
    {
        // Memory error
        return -ENOMEM;
    }
    int accessResult = fudr_access(gdrive_path_get_dirname(pGpath), W_OK);
    gdrive_path_free(pGpath);
    if (accessResult)
    {
        // Access check failed
        return accessResult;
    }
    
    // Create the file
    int error = 0;
    char* fileId = gdrive_file_new(path, false, &error);
    if (fileId == NULL)
    {
        // Some error occurred
        return -error;
    }
    
    // TODO: If fudr_chmod is ever implemented, change the file permissions 
    // using the (currently unused) mode parameter we were given.
    
    // File was successfully created. Open it.
    fi->fh = (uint64_t) gdrive_file_open(fileId, O_RDWR, &error);
    free(fileId);
    
    return -error;
}

static void fudr_destroy(void* private_data)
{
    // Silence compiler warning about unused parameter
    (void) private_data;
    
    gdrive_cleanup();
}

//static int 
//fudr_fallocate(const char* path, int mode, off_t offset, off_t len, 
//               struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}

static int 
fudr_fgetattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi)
{
    Gdrive_File* fh = (Gdrive_File*) fi->fh;
    const Gdrive_Fileinfo* pFileinfo = (fi->fh == (uint64_t) NULL) ? 
        NULL : gdrive_file_get_info(fh);
    
    if (pFileinfo == NULL)
    {
        // Invalid file handle
        return -EBADF;
    }
    
    return fudr_stat_from_fileinfo(pFileinfo, strcmp(path, "/") == 0, stbuf);
}

//static int fudr_flock(const char* path, struct fuse_file_info* fi, int op)
//{
//    
//}

//static int fudr_flush(const char* path, struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}

static int 
fudr_fsync(const char* path, int isdatasync, struct fuse_file_info* fi)
{
    // Distinguishing between data-only and data-and-metadata syncs doesn't
    // really help us, so ignore isdatasync. Ignore path since we should have
    // a Gdrive file handle.
    (void) isdatasync;
    (void) path;
    
    if (fi->fh == (uint64_t) NULL)
    {
        // Bad file handle
        return -EBADF;
    }
    
    return gdrive_file_sync((Gdrive_File*) fi->fh);
}

//static int 
//fudr_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}

static int 
fudr_ftruncate(const char* path, off_t size, struct fuse_file_info* fi)
{
    // Suppress unused parameter compiler warnings
    (void) path;
    
    Gdrive_File* fh = (Gdrive_File*) fi->fh;
    if (fh == NULL)
    {
        // Invalid file handle
        return -EBADF;
    }
    
    // Need write access to the file
    int accessResult = fudr_access(path, W_OK);
    if (accessResult)
        return accessResult;
    
    return gdrive_file_truncate(fh, size);
}

static int fudr_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));
    
    char* fileId = gdrive_filepath_to_id(path);
    if (fileId == NULL)
    {
        // File not found
        return -ENOENT;
    }
    
    const Gdrive_Fileinfo* pFileinfo = gdrive_finfo_get_by_id(fileId);
    free(fileId);
    if (pFileinfo == NULL)
    {
        // An error occurred.
        return -ENOENT;
    }    
    
    return fudr_stat_from_fileinfo(pFileinfo, strcmp(path, "/") == 0, stbuf);
}

//static int 
//fudr_getxattr(const char* path, const char* name, char* value, size_t size)
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

//static int 
//fudr_ioctl(const char* path, int cmd, void* arg, struct fuse_file_info* fi, 
//           unsigned int flags, void* data)
//{
//    return -ENOSYS;
//}

static int fudr_link(const char* from, const char* to)
{
    Gdrive_Path* pOldPath = gdrive_path_create(from);
    Gdrive_Path* pNewPath = gdrive_path_create(to);
    
    // Determine whether the file already exists
    char* dummyFileId = gdrive_filepath_to_id(to);
    free(dummyFileId);
    if (dummyFileId != NULL)
    {
        return -EEXIST;
    }
    
    // Google Drive supports a file with multiple parents - that is, a file with
    // multiple hard links that all have the same base name.
    if (strcmp(gdrive_path_get_basename(pOldPath), 
               gdrive_path_get_basename(pNewPath))
            )
    {
        // Basenames differ, not supported
        return -ENOENT;
    }
    
    // Need write access in the target directory
    int accessResult = fudr_access(gdrive_path_get_dirname(pNewPath), W_OK);
    if (accessResult)
        return accessResult;
    
    char* fileId = gdrive_filepath_to_id(from);
    if (!fileId)
    {
        // Original file does not exist
        return -ENOENT;
    }
    char* newParentId = 
        gdrive_filepath_to_id(gdrive_path_get_dirname(pNewPath));
    gdrive_path_free(pOldPath);
    gdrive_path_free(pNewPath);
    if (!newParentId)
    {
        // New directory doesn't exist
        free(fileId);
        return -ENOENT;
    }
    
    int returnVal = gdrive_add_parent(fileId, newParentId);
    
    free(fileId);
    free(newParentId);
    return returnVal;
}

//static int fudr_listxattr(const char* path, char* list, size_t size)
//{
//    return -ENOSYS;
//}
//static int 
//fudr_lock(const char* path, struct fuse_file_info* fi, int cmd, 
//          struct flock* locks)
//{
//    return -ENOSYS;
//}

static int fudr_mkdir(const char* path, mode_t mode)
{
    // Silence compiler warning for unused variable. If and when chmod is 
    // implemented, this should be removed.
    (void) mode;
    
    // Determine whether the folder already exists, 
    char* dummyFileId = gdrive_filepath_to_id(path);
    free(dummyFileId);
    if (dummyFileId != NULL)
    {
        return -EEXIST;
    }
    
    // Need write access to the parent directory
    Gdrive_Path* pGpath = gdrive_path_create(path);
    if (!pGpath)
    {
        // Memory error
        return -ENOMEM;
    }
    int accessResult = fudr_access(gdrive_path_get_dirname(pGpath), W_OK);
    gdrive_path_free(pGpath);
    if (accessResult)
        return accessResult;
    
    // Create the folder
    int error = 0;
    dummyFileId = gdrive_file_new(path, true, &error);
    free(dummyFileId);
    
    // TODO: If fudr_chmod is ever implemented, change the folder permissions 
    // using the (currently unused) mode parameter we were given.
    
    return -error;
}

//static int fudr_mknod(const char* path, mode_t mode, dev_t rdev)
//{
//    return -ENOSYS;
//}

static int fudr_open(const char *path, struct fuse_file_info *fi)
{
    // Get the file ID
    char* fileId = gdrive_filepath_to_id(path);
    if (fileId == NULL)
    {
        // File not found
        return -ENOENT;
    }
    
    // Confirm permissions
    unsigned int modeNeeded = 0;
    if (fi->flags & (O_RDONLY | O_RDWR))
    {
        modeNeeded |= R_OK;
    }
    if (fi->flags & (O_WRONLY | O_RDWR))
    {
        modeNeeded |= W_OK;
    }
    if (!modeNeeded)
        modeNeeded = F_OK;
    int accessResult = fudr_access(path, modeNeeded);
    if (accessResult)
        return accessResult;
    
    // Open the file
    int error = 0;
    Gdrive_File* pFile = gdrive_file_open(fileId, fi->flags, &error);
    free(fileId);
    
    if (pFile == NULL)
    {
        // An error occurred.
        return -error;
    }
    
    // Store the file handle
    fi->fh = (uint64_t) pFile;
    return 0;
}

//static int fudr_opendir(const char* path, struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}
//static int 
//fudr_poll(const char* path, struct fuse_file_info* fi, 
//          struct fuse_pollhandle* ph, unsigned* reventsp)
//{
//    return -ENOSYS;
//}

static int 
fudr_read(const char *path, char *buf, size_t size, off_t offset,
          struct fuse_file_info *fi)
{
    // Silence compiler warning about unused parameter
    (void) path;
    
    // Check for read access
    int accessResult = fudr_access(path, R_OK);
    if (accessResult)
        return accessResult;
    
    Gdrive_File* pFile = (Gdrive_File*) fi->fh;
    
    return gdrive_file_read(pFile, buf, size, offset);
}

//static int 
//fudr_read_buf(const char* path, struct fuse_bufvec **bufp, 
//              size_t size, off_t off, struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}

static int 
fudr_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    // Suppress warnings for unused function parameters
    (void) offset;
    (void) fi;
    
    char* folderId = gdrive_filepath_to_id(path);
    if (folderId == NULL)
    {
        return -ENOENT;
    }
    
    // Check for read access
    int accessResult = fudr_access(path, R_OK);
    if (accessResult)
    {
        free(folderId);
        return accessResult;
    }
    
    Gdrive_Fileinfo_Array* pFileArray = 
            gdrive_folder_list(folderId);
    free(folderId);
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

static int fudr_release(const char* path, struct fuse_file_info *fi)
{
    // Suppress unused parameter warning
    (void) path;
    
    if (fi->fh == (uint64_t) NULL)
    {
        // Bad file handle
        return -EBADF;
    }
    
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

static int fudr_rename(const char* from, const char* to)
{
    // Neither from nor to should be the root directory
    char* rootId = gdrive_filepath_to_id("/");
    if (!rootId)
    {
        // Memory error
        return -ENOMEM;
    }
    
    char* fromFileId = gdrive_filepath_to_id(from);
    if (!fromFileId)
    {
        // from doesn't exist
        free(rootId);
        return -ENOENT;
    }
    if (!strcmp(fromFileId, rootId))
    {
        // from is root
        free(fromFileId);
        free(rootId);
        return -EBUSY;
    }
    char* toFileId = gdrive_filepath_to_id(to); // May be NULL.
    
    // Special handling if the destination exists
    if (toFileId)
    {
        if (!strcmp(toFileId, rootId))
        {
            // to is root
            free(toFileId);
            free(fromFileId);
            free(rootId);
            return -EBUSY;
        }
        free(rootId);
        
        // If from and to are hard links to the same file, do nothing and 
        // return success.
        if (!strcmp(fromFileId, toFileId))
        {
            free(toFileId);
            free(fromFileId);
            return 0;
        }
        
        // If the source is a directory, destination must be an empty directory
        const Gdrive_Fileinfo* pFromInfo = gdrive_finfo_get_by_id(fromFileId);
        if (pFromInfo && pFromInfo->type == GDRIVE_FILETYPE_FOLDER)
        {
            const Gdrive_Fileinfo* pToInfo = gdrive_finfo_get_by_id(toFileId);
            if (pToInfo && pToInfo->type != GDRIVE_FILETYPE_FOLDER)
            {
                // Destination is not a directory
                free(toFileId);
                free(fromFileId);
                return -ENOTDIR;
            }
            if (pToInfo && pToInfo->nChildren > 0)
            {
                // Destination is not empty
                free(toFileId);
                free(fromFileId);
                return -ENOTEMPTY;
            }
        }
        
        // Need write access for the destination
        int accessResult = fudr_access(to, W_OK);
        if (accessResult)
        {
            free(toFileId);
            free(fromFileId);
            return -EACCES;
        }
    }
    else 
    {
        free(rootId);
    }
    
    Gdrive_Path* pFromPath = gdrive_path_create(from);
    if (!pFromPath)
    {
        // Memory error
        free(fromFileId);
        return -ENOMEM;
    }
    Gdrive_Path* pToPath = gdrive_path_create(to);
    if (!pToPath)
    {
        // Memory error
        gdrive_path_free(pFromPath);
        free(fromFileId);
        return -ENOMEM;
    }
    
    char* fromParentId = 
        gdrive_filepath_to_id(gdrive_path_get_dirname(pFromPath));
    if (!fromParentId)
    {
        // from path doesn't exist
        gdrive_path_free(pToPath);
        gdrive_path_free(pFromPath);
        free(fromFileId);
        return -ENOENT;
    }
    char* toParentId = 
        gdrive_filepath_to_id(gdrive_path_get_dirname(pToPath));
    if (!toParentId)
    {
        // from path doesn't exist
        free(fromParentId);
        gdrive_path_free(pToPath);
        gdrive_path_free(pFromPath);
        free(fromFileId);
        return -ENOENT;
    }
    
    // Need write access in the destination parent directory
    int accessResult = fudr_access(gdrive_path_get_dirname(pToPath), W_OK);
    if (accessResult)
    {
        free(toParentId);
        free(fromParentId);
        gdrive_path_free(pToPath);
        gdrive_path_free(pFromPath);
        free(fromFileId);
        return accessResult;
    }
    
    // If the directories are different, create a new hard link and delete
    // the original. Compare the actual file IDs of the parents, not the paths,
    // because different paths could refer to the same directory.
    if (strcmp(fromParentId, toParentId))
    {
        int result = gdrive_add_parent(fromFileId, toParentId);
        if (result != 0)
        {
            // An error occurred
            free(toParentId);
            free(fromParentId);
            gdrive_path_free(pToPath);
            gdrive_path_free(pFromPath);
            free(fromFileId);
            return result;
        }
        result = fudr_unlink(from);
        if (result != 0)
        {
            // An error occurred
            free(toParentId);
            free(fromParentId);
            gdrive_path_free(pToPath);
            gdrive_path_free(pFromPath);
            free(fromFileId);
            return result;
        }
    }
    
    int returnVal = 0;
    
    // If the basenames are different, change the basename. NOTE: If there are
    // any other hard links to the file, this will also change their names.
    const char* fromBasename = gdrive_path_get_basename(pFromPath);
    const char* toBasename = gdrive_path_get_basename(pToPath);
    if (strcmp(fromBasename, toBasename))
    {
        returnVal = gdrive_change_basename(fromFileId, toBasename);
    }
    
    // If successful, and if to already existed, delete it
    if (toFileId && !returnVal)
    {
        returnVal = fudr_rm_file_or_dir_by_id(toFileId, toParentId);
    }
    
    
    free(toFileId);
    free(toParentId);
    free(fromParentId);
    gdrive_path_free(pFromPath);
    gdrive_path_free(pToPath);
    free(fromFileId);
    return returnVal;
}

static int fudr_rmdir(const char* path)
{
    // Can't delete the root directory
    if (strcmp(path, "/") == 0)
    {
        return -EBUSY;
    }
    
    char* fileId = gdrive_filepath_to_id(path);
    if (fileId == NULL)
    {
        // No such file
        return -ENOENT;
    }
    
    // Make sure path refers to an empty directory
    const Gdrive_Fileinfo* pFileinfo = gdrive_finfo_get_by_id(fileId);
    if (pFileinfo == NULL)
    {
        // Couldn't retrieve file info
        free(fileId);
        return -ENOENT;
    }
    if (pFileinfo->type != GDRIVE_FILETYPE_FOLDER)
    {
        // Not a directory
        free(fileId);
        return -ENOTDIR;
    }
    if (pFileinfo->nChildren > 0)
    {
        // Not empty
        free(fileId);
        return -ENOTEMPTY;
    }
    
    // Need write access
    int accessResult = fudr_access(path, W_OK);
    if (accessResult)
    {
        free(fileId);
        return accessResult;
    }
    
    // Get the parent ID
    Gdrive_Path* pGpath = gdrive_path_create(path);
    if (pGpath == NULL)
    {
        // Memory error
        free(fileId);
        return -ENOMEM;
    }
    char* parentId = 
        gdrive_filepath_to_id(gdrive_path_get_dirname(pGpath));
    gdrive_path_free(pGpath);
    
    int returnVal = fudr_rm_file_or_dir_by_id(fileId, parentId);
    free(parentId);
    free(fileId);
    return returnVal;
}

//static int 
//fudr_setxattr(const char* path, const char* name, const char* value, 
//              size_t size, int flags)
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
    char* fileId = gdrive_filepath_to_id(path);
    if (fileId == NULL)
    {
        // File not found
        return -ENOENT;
    }
    
    // Need write access
    int accessResult = fudr_access(path, W_OK);
    if (accessResult)
    {
        free(fileId);
        return accessResult;
    }
    
    // Open the file
    int error = 0;
    Gdrive_File* fh = gdrive_file_open(fileId, O_RDWR, &error);
    free(fileId);
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

static int fudr_unlink(const char* path)
{
    char* fileId = gdrive_filepath_to_id(path);
    if (fileId == NULL)
    {
        // No such file
        return -ENOENT;
    }
    
    // Need write access
    int accessResult = fudr_access(path, W_OK);
    if (accessResult)
    {
        free(fileId);
        return accessResult;
    }
    
    Gdrive_Path* pGpath = gdrive_path_create(path);
    if (pGpath == NULL)
    {
        // Memory error
        free(fileId);
        return -ENOMEM;
    }
    char* parentId = gdrive_filepath_to_id(gdrive_path_get_dirname(pGpath));
    gdrive_path_free(pGpath);
    
    int returnVal = fudr_rm_file_or_dir_by_id(fileId, parentId);
    free(parentId);
    free(fileId);
    return returnVal;
}

//static int fudr_utime()
//{
//    return -ENOSYS;
//}

static int fudr_utimens(const char* path, const struct timespec ts[2])
{
    char* fileId = gdrive_filepath_to_id(path);
    if (fileId == NULL)
    {
        return -ENOENT;
    }
    
    int error = 0;
    Gdrive_File* fh = gdrive_file_open(fileId, O_RDWR, &error);
    free(fileId);
    if (fh == NULL)
    {
        return -error;
    }
    
    if (ts[0].tv_nsec == UTIME_NOW)
    {
        error = gdrive_file_set_atime(fh, NULL);
    }
    else if (ts[0].tv_nsec != UTIME_OMIT)
    {
        error = gdrive_file_set_atime(fh, &(ts[0]));
    }
    
    if (error != 0)
    {
        gdrive_file_close(fh, O_RDWR);
        return error;
    }
    
    if (ts[1].tv_nsec == UTIME_NOW)
    {
        gdrive_file_set_mtime(fh, NULL);
    }
    else if (ts[1].tv_nsec != UTIME_OMIT)
    {
        gdrive_file_set_mtime(fh, &(ts[1]));
    }
    
    gdrive_file_close(fh, O_RDWR);
    return error;
}

static int 
fudr_write(const char* path, const char *buf, size_t size, off_t offset, 
           struct fuse_file_info* fi)
{
    // Avoid compiler warning for unused variable
    (void) path;
    
    // Check for write access
    int accessResult = fudr_access(path, W_OK);
    if (accessResult)
        return accessResult;
    
    Gdrive_File* fh = (Gdrive_File*) fi->fh;
    if (fh == NULL)
    {
        // Bad file handle
        return -EBADFD;
    }
    
    return gdrive_file_write(fh, buf, size, offset);
}

//static int 
//fudr_write_buf(const char* path, struct fuse_bufvec* buf, off_t off, 
//               struct fuse_file_info* fi)
//{
//    return -ENOSYS;
//}


static struct fuse_operations fo = {
    .access         = fudr_access,
    .bmap           = NULL, //fudr_bmap,        // Not needed
    .chmod          = NULL, //fudr_chmod,       // Might consider later
    .chown          = NULL, //fudr_chown,       // Might consider later
    .create         = fudr_create,
    .destroy        = fudr_destroy,
    .fallocate      = NULL, //fudr_fallocate,   // Not needed
    .fgetattr       = fudr_fgetattr,
    .flock          = NULL, //fudr_flock,       // Not needed
    .flush          = NULL, //fudr_flush,       // Not needed
    .fsync          = fudr_fsync,
    .fsyncdir       = NULL, //fudr_fsyncdir,    // Not needed
    .ftruncate      = fudr_ftruncate,
    .getattr        = fudr_getattr,
    .getxattr       = NULL, //fudr_getxattr,    // Not needed
    .init           = fudr_init,
    .ioctl          = NULL, //fudr_ioctl,       // Not needed
    .link           = fudr_link,
    .listxattr      = NULL, //fudr_listxattr,   // Not needed
    .lock           = NULL, //fudr_lock,        // Not needed
    .mkdir          = fudr_mkdir,
    .mknod          = NULL, //fudr_mknod,       // Not needed
    .open           = fudr_open,
    .opendir        = NULL, //fudr_opendir,     // Not needed
    .poll           = NULL, //fudr_poll,        // Not needed
    .read           = fudr_read,
    .read_buf       = NULL, //fudr_read_buf,    // ???
    .readdir        = fudr_readdir,
    .readlink       = NULL, //fudr_readlink,    // Might consider later
    .release        = fudr_release,
    .releasedir     = NULL, //fudr_releasedir,  // Not needed
    .removexattr    = NULL, //fudr_removexattr, // Not needed
    .rename         = fudr_rename,
    .rmdir          = fudr_rmdir,
    .setxattr       = NULL, //fudr_setxattr,    // Not needed
    .statfs         = fudr_statfs,
    .symlink        = NULL, //fudr_symlink,     // Might consider later
    .truncate       = fudr_truncate,
    .unlink         = fudr_unlink,
    .utime          = NULL, //fudr_utime,       // Not needed
    .utimens        = fudr_utimens,
    .write          = fudr_write,
    .write_buf      = NULL, //fudr_write_buf,   // ????
};


/*
 * 
 */
int main(int argc, char** argv) 
{
    
    
    Fudr_Options* pOptions = fudr_options_create(argc, argv);
    if (!pOptions)
    {
        fputs("Could not load command line option parser, aborting\n", stderr);
        return 1;
    }
    
    if (gdrive_init(pOptions->gdrive_access, pOptions->gdrive_auth_file, 
                    pOptions->gdrive_cachettl, 
                    pOptions->gdrive_interaction_type, 
                    pOptions->gdrive_chunk_size, pOptions->gdrive_max_chunks)
            )
    {
        fputs("Could not set up a Google Drive connection.\n", stderr);
        return 1;
    }
    int returnVal;
    returnVal = fuse_main(pOptions->fuse_argc, pOptions->fuse_argv, &fo, 
                          (void*) ((pOptions->dir_perms << 9) + 
                          pOptions->file_perms));
    
    fudr_options_free(pOptions);
    return returnVal;
}

#endif	/*__GDRIVE_TEST__*/

