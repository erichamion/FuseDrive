

#include "gdrive-util.h"

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>

typedef struct Gdrive_Path
{
    char* dirCpy;
    const char* dirnamePart;
    char* baseCpy;
    const char* basenamePart;
} Gdrive_Path;

Gdrive_Path* gdrive_path_create(const char* path)
{
    Gdrive_Path* returnVal = malloc(sizeof(Gdrive_Path));
    if (returnVal == NULL)
    {
        // Memory error
        return NULL;
    }
    
    // Make new copies of path because dirname() and some versions of basename()
    // may modify the arguments
    size_t pathSize = strlen(path) + 1;
    returnVal->dirCpy = malloc(pathSize);
    if (returnVal->dirCpy == NULL)
    {
        // Memory error
        free(returnVal);
        return NULL;
    }
    memcpy(returnVal->dirCpy, path, pathSize);
    returnVal->baseCpy = malloc(pathSize);
    if (returnVal->baseCpy == NULL)
    {
        // Memory error
        free(returnVal->dirCpy);
        free(returnVal);
        return NULL;
    }
    memcpy(returnVal->baseCpy, path, pathSize);
    
    // Find the parent folder and the base filename
    returnVal->dirnamePart = dirname(returnVal->dirCpy);
    returnVal->basenamePart = basename(returnVal->baseCpy);
    
    return returnVal;
}

const char* gdrive_path_get_dirname(const Gdrive_Path* gpath)
{
    return gpath->dirnamePart;
}

const char* gdrive_path_get_basename(const Gdrive_Path* gpath)
{
    return gpath->basenamePart;
}

void gdrive_path_free(Gdrive_Path* gpath)
{
    free(gpath->dirCpy);
    gpath->dirCpy = NULL;
    gpath->dirnamePart = NULL;
    free(gpath->baseCpy);
    gpath->baseCpy = NULL;
    gpath->basenamePart = NULL;
    
    free(gpath);
}

long _gdrive_divide_round_up(long dividend, long divisor)
{
    // Could use ceill() or a similar function for this, but I don't  know 
    // whether there might be some values that don't convert exactly between
    // long int and long double and back.
    
    // Integer division rounds down.  If there's a remainder, add 1.
    return (dividend % divisor == 0) ? 
        (dividend / divisor) : 
        (dividend / divisor + 1);
}

FILE* gdrive_power_fopen(const char* path, const char* mode)
{
    // Any files we create would be authentication and possibly (not currently
    // implemented) configuration files. These should be visible only to the
    // user.
    mode_t oldUmask = umask(S_IRGRP | S_IWGRP | S_IXGRP | 
                            S_IROTH | S_IWOTH | S_IXOTH);
    
    Gdrive_Path* pGpath = gdrive_path_create(path);
    const char* dirname = gdrive_path_get_dirname(pGpath);
    
    FILE* returnVal = NULL;
    
    // Does the parent directory exist?
    if (access(dirname, F_OK))
    {
        // Directory doesn't exist, need to create it.
        if (!gdrive_recursive_mkdir(dirname))
        {
            // Successfully created directory
            returnVal = fopen(path, mode);
        }
        // else do nothing, cleanup and return failure
    }
    else
    {
        // Directory exists, just need to open the file
        returnVal = fopen(path, mode);
    }
    
    umask(oldUmask);
    gdrive_path_free(pGpath);
    return returnVal;
}

int gdrive_recursive_mkdir(const char* path)
{
    Gdrive_Path* pGpath = gdrive_path_create(path);
    const char* parentDir = gdrive_path_get_dirname(pGpath);
    
    int returnVal;
    // Does the parent directory exist?
    if (access(parentDir, F_OK))
    {
        // Directory doesn't exist, need to create it.
        returnVal = gdrive_recursive_mkdir(parentDir);
        if (!returnVal)
        {
            // Successfully created directory
            returnVal = mkdir(path, 0755);
        }
        // else do nothing, cleanup and return failure
    }
    else
    {
        // Directory exists, just need to open the file
        returnVal = mkdir(path, 0755);
    }
    
    gdrive_path_free(pGpath);
    return returnVal;
}






void dumpfile(FILE* fh, FILE* dest)
{
    long oldPos = ftell(fh);
    if (fseek(fh, 0, SEEK_SET) != 0) return;
    int bytesRead;
    char buf[1024];
    while ((bytesRead = fread(buf, 1, 1024, fh)) > 0)   // Intentional assignment
    {
        fwrite(buf, 1, bytesRead, dest);
    }
    
    fseek(fh, oldPos, SEEK_SET);
}

// For temporary debugging only. This will have memory leaks
char* display_epochtime(time_t epochTime)
{
    struct tm* pTime = gmtime(&epochTime);
    size_t size = 50;
    char* result = malloc(50);
    while (!strftime(result, size, "%Y-%m-%dT%H:%M:%S", pTime))
    {
        size *= 2;
        result = realloc(result, size);
    }
    return result;
}
char* display_timespec(const struct timespec* tm)
{
    char* baseTime = display_epochtime(tm->tv_sec);
    size_t newSize = strlen(baseTime) + 11;
    char* result = malloc(newSize);
    snprintf(result, newSize, "%s.%09li", baseTime, tm->tv_nsec);
    free(baseTime);
    return result;
}
char* display_epochtime_local(time_t epochTime)
{
    struct tm* pTime = localtime(&epochTime);
    size_t size = 50;
    char* result = malloc(50);
    while (!strftime(result, size, "%Y-%m-%dT%H:%M:%S", pTime))
    {
        size *= 2;
        result = realloc(result, size);
    }
    return result;
}
char* display_timespec_local(const struct timespec* tm)
{
    char* baseTime = display_epochtime_local(tm->tv_sec);
    size_t newSize = strlen(baseTime) + 11;
    char* result = malloc(newSize);
    snprintf(result, newSize, "%s.%09li", baseTime, tm->tv_nsec);
    free(baseTime);
    return result;
}