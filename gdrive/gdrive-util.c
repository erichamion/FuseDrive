

#include "gdrive-util.h"

#include <stdlib.h>
#include <string.h>
#include <libgen.h>

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