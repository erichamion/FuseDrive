

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <assert.h>

#include "fuse-drive-options.h"


/*
 * Constants needed only within this file
 */
#define OPTION_ACCESS 'a'
#define OPTION_CONFIG 'c'
#define OPTION_INTERACTION 'i'
#define OPTION_FILEPERM 'p'
#define OPTION_DIRPERM 'd'
#define OPTION_CACHETTL 500
#define OPTION_CHUNKSIZE 501
#define OPTION_MAXCHUNKS 502
#define OPTION_STRING "a:c:i:p:d:"

#define DEFAULT_GDRIVE_ACCESS GDRIVE_ACCESS_WRITE
#define DEFAULT_AUTH_BASENAME ".auth"
#define DEFAULT_AUTH_RELPATH "fuse-drive"
#define DEFAULT_CACHETTL 30
#define DEFAULT_INTERACTION GDRIVE_INTERACTION_STARTUP
#define DEFAULT_CHUNKSIZE GDRIVE_BASE_CHUNK_SIZE * 4
#define DEFAULT_MAXCHUNKS 15
#define DEFAULT_FILEPERMS 0644
#define DEFAULT_DIRPERMS 07777


/**
 * Declarations of static functions
 */

static Fudr_Options* fudr_options_default();

static char* fudr_options_get_default_auth_file();

static bool fudr_options_set_access(Fudr_Options* pOptions, const char* arg);

static bool fudr_options_set_config(Fudr_Options* pOptions, const char* arg);

static bool fudr_options_set_interaction(Fudr_Options* pOptions, 
                                         const char* arg);

static bool fudr_options_set_fileperm(Fudr_Options* pOptions, const char* arg);

static bool fudr_options_set_dirperm(Fudr_Options* pOptions, const char* arg);

static bool fudr_options_set_cachettl(Fudr_Options* pOptions, const char* arg);

static bool fudr_options_set_chunksize(Fudr_Options* pOptions, const char* arg);

static bool fudr_options_set_maxchunks(Fudr_Options* pOptions, const char* arg);

static bool fudr_options_set_failed(Fudr_Options* pOptions, int arg, 
                                    const struct option* longopts, 
                                    int longIndex);

static void fudr_options_make_errormsg(char** pDest, const char* fmtStr, 
                                       const char* arg);


/**
 * Non-static function implementations
 */

/**
 * fudr_options_create: Fills in a Fudr_Options struct based on command-line
 *                      arguments (or simulated command-line arguments)
 * @param argc (int): Argument count, as in main().
 * @param argv (char**): Array of argument values, as in main().
 * @return (Fudr_Options*): Pointer to a struct containing the options. Any
 *                          options that are not explicitly set will have
 *                          reasonable defaults. If the error member of the
 *                          struct is true, then there was a problem 
 *                          interpreting the arguments, and exiting is 
 *                          recommended. In this case, the errorMsg member will
 *                          contain a message that may be displayed to the user.
 *                          The caller is responsible for passing the returned
 *                          pointer to fudr_options_free().
 */
Fudr_Options* fudr_options_create(int argc, char** argv)
{
    // Initialize all options to their default values
    Fudr_Options* pOptions = fudr_options_default();
    
    if (pOptions)
    {
        // Set up the long options
        struct option longopts[] = 
        {
            {
                .name = "access",
                .has_arg = required_argument,
                .flag = NULL,
                .val = OPTION_ACCESS
            },
            {
                .name = "config",
                .has_arg = required_argument,
                .flag = NULL,
                .val = OPTION_CONFIG
            },
            {
                .name = "cache-time",
                .has_arg = required_argument,
                .flag = NULL,
                .val = OPTION_CACHETTL
            },
            {
                .name = "interaction",
                .has_arg = required_argument,
                .flag = NULL,
                .val = OPTION_INTERACTION
            },
            {
                .name = "chunk-size",
                .has_arg = required_argument,
                .flag = NULL,
                .val = OPTION_CHUNKSIZE
            },
            {
                .name = "max-chunks",
                .has_arg = required_argument,
                .flag = NULL,
                .val = OPTION_MAXCHUNKS
            },
            {
                .name = "file-perm",
                .has_arg = required_argument,
                .flag = NULL,
                .val = OPTION_FILEPERM
            },
            {
                .name = "dir-perm",
                .has_arg = required_argument,
                .flag = NULL,
                .val = OPTION_DIRPERM
            },
            {
                // End the array with an 
                // all-zero element
                0
            }
        };
        
        opterr = 0;
        int currentArg;
        bool hasError = false;
        int longoptIndex = -1;
        while ((currentArg = getopt_long(argc, argv, OPTION_STRING, 
                                         longopts, &longoptIndex)) != -1)
        {
            switch (currentArg)
            {
                case OPTION_ACCESS:
                    // Set Google Drive access level
                    hasError = fudr_options_set_access(pOptions, optarg);
                    break;
                case OPTION_CONFIG:
                    // Set config file (auth file)
                    hasError = fudr_options_set_config(pOptions, optarg);
                    break;
                case OPTION_INTERACTION:
                    // Set the interaction type
                    hasError = fudr_options_set_interaction(pOptions, optarg);
                    break;
                case OPTION_FILEPERM:
                    // Set the file permissions
                    hasError = fudr_options_set_fileperm(pOptions, optarg);
                    break;
                case OPTION_DIRPERM:
                    // Set directory permissions
                    hasError = fudr_options_set_dirperm(pOptions, optarg);
                    break;
                case OPTION_CACHETTL:
                    // Set cache TTL
                    hasError = fudr_options_set_cachettl(pOptions, optarg);
                    break;
                case OPTION_CHUNKSIZE:
                    // Set chunk size
                    hasError = fudr_options_set_chunksize(pOptions, optarg);
                    break;
                case OPTION_MAXCHUNKS:
                    // Set max chunks
                    hasError = fudr_options_set_maxchunks(pOptions, optarg);
                    break;
                case '?': 
                    // Fall through to default
                    default:
                        hasError = 
                                fudr_options_set_failed(pOptions, optopt, 
                                                        longopts, longoptIndex);
            }
            
            if (hasError)
            {
                // Stop processing after an error
                return pOptions;
            }
        }
        
        if (pOptions->dir_perms == DEFAULT_DIRPERMS)
        {
            // Default directory permissions start by coping the file 
            // permissions, but anybody who has read permission also gets
            // execute permission.
            pOptions->dir_perms = pOptions->file_perms;
            unsigned int read_perms = pOptions->dir_perms & 0444;
            pOptions->dir_perms |= (read_perms >> 2);
        }
        
        
        
        // Pass on unrecognized options to FUSE, but make a copy first.
        // We need to add an argv[0], and we'll be adding some extra options to
        // the end, so we can't just use the passed-in array.
        if (optind < argc && !strcmp(argv[optind], "--"))
        {
            // If a "--" end-of-arguments is found, skip past it
            optind++;
        }
        pOptions->fuse_argc = argc - optind + 1;
        pOptions->fuse_argv = malloc((pOptions->fuse_argc + 2) * sizeof(char*));
        if (!pOptions->fuse_argv)
        {
            // Memory error
            fudr_options_free(pOptions);
            return NULL;
        }
        pOptions->fuse_argv[0] = argv[0];
        for (int i = 1; i < pOptions->fuse_argc; i++)
        {
            pOptions->fuse_argv[i] = argv[i + optind - 1];
        }
        
        
        
        // If we might need to interact with the user, need to add the
        // foreground option. The foreground option also changes other behavior
        // (specifically, the working directory is different). Since we need the
        // option sometimes, always add it to be consistent.
        pOptions->fuse_argv[pOptions->fuse_argc++] = "-f";
        
        // Enforce single-threaded mode
        pOptions->fuse_argv[pOptions->fuse_argc++] = "-s";
    }
    
    return pOptions;
}

/**
 * fudr_options_free:   Safely frees all resources associated with a 
 *                      Fudr_Options struct.
 * @param pOptions (Fudr_Options*): A struct previously returned from
 *                                  fudr_options_create(). It is safe to pass
 *                                  a NULL pointer.
 */
void fudr_options_free(Fudr_Options* pOptions)
{
    if (!pOptions)
    {
        // Nothing to do.
        return;
    }
    
    pOptions->gdrive_access = 0;
    free(pOptions->gdrive_auth_file);
    pOptions->gdrive_auth_file = NULL;
    pOptions->gdrive_cachettl = 0;
    pOptions->gdrive_interaction_type = 0;
    pOptions->gdrive_chunk_size = 0;
    pOptions->gdrive_max_chunks = 0;
    pOptions->file_perms = 0;
    pOptions->dir_perms = 0;
    free(pOptions->fuse_argv);
    pOptions->fuse_argv = NULL;
    pOptions->fuse_argc = 0;
    pOptions->error = false;
    free(pOptions->errorMsg);
    pOptions->errorMsg = NULL;
    free(pOptions);
}


/**
 * Static function implementations
 */

/**
 * Sets up default options
 * @return (Fudr_Options*): Must be freed with fudr_options_free()
 */
static Fudr_Options* fudr_options_default()
{
    Fudr_Options* pOptions = malloc(sizeof(Fudr_Options));
    if (!pOptions)
    {
        // Memory error
        return NULL;
    }
    
    pOptions->gdrive_access = DEFAULT_GDRIVE_ACCESS;
    pOptions->gdrive_auth_file = fudr_options_get_default_auth_file();
    pOptions->gdrive_cachettl = DEFAULT_CACHETTL;
    pOptions->gdrive_interaction_type = DEFAULT_INTERACTION;
    pOptions->gdrive_chunk_size = DEFAULT_CHUNKSIZE;
    pOptions->gdrive_max_chunks = DEFAULT_MAXCHUNKS;
    pOptions->file_perms = DEFAULT_FILEPERMS;
    pOptions->dir_perms = DEFAULT_DIRPERMS;
    pOptions->fuse_argv = NULL;
    pOptions->fuse_argc = 0;
    pOptions->error = false;
    pOptions->errorMsg = NULL;
    
    return pOptions;
}

/**
 * Gets the default path for the config/auth file.
 * @return (char*): A string containing "<HOME-DIRECTORY>/"
 *                  "<FUDR_DEFAULT_AUTH_RELPATH>/<FUDR_DEFAULT_AUTH_BASENAME>"
 *                  on success, or NULL on error. The returned memory must be 
 *                  freed by the caller.
 */
static char* fudr_options_get_default_auth_file()
{
    // Get the user's home directory
    const char* homedir = getenv("HOME");
    if (!homedir)
    {
        homedir = getpwuid(getuid())->pw_dir;
    }
    
    // Append the relative path and basename to the home directory
    char* auth_file = malloc(strlen(homedir) + strlen(DEFAULT_AUTH_RELPATH)
                            + strlen(DEFAULT_AUTH_BASENAME) + 3);
    if (auth_file)
    {
        strcpy(auth_file, homedir);
        strcat(auth_file, "/");
        strcat(auth_file, DEFAULT_AUTH_RELPATH);
        strcat(auth_file, "/");
        strcat(auth_file, DEFAULT_AUTH_BASENAME);
    }
    // else auth_file is NULL, return NULL for error
    
    return auth_file;
}

/**
 * Set the Google Drive access level
 * @param pOptions
 * @param arg
 * @return false on success, true on error
 */
static bool fudr_options_set_access(Fudr_Options* pOptions, const char* arg)
{
    // Nothing should be NULL
    assert(pOptions && arg);
    
    if (!strcmp(arg, "meta"))
    {
        pOptions->gdrive_access = GDRIVE_ACCESS_META;
    }
    else if (!strcmp(arg, "read"))
    {
        pOptions->gdrive_access = GDRIVE_ACCESS_READ;
    }
    else if (!strcmp(arg, "write"))
    {
        pOptions->gdrive_access = GDRIVE_ACCESS_WRITE;
    }
    else if (!strcmp(arg, "apps"))
    {
        pOptions->gdrive_access = GDRIVE_ACCESS_APPS;
    }
    else if (!strcmp(arg, "all"))
    {
        pOptions->gdrive_access = GDRIVE_ACCESS_ALL;
    }
    else
    {
        pOptions->error = true;
        const char* fmtStr = "Unrecognized access level '%s'. Valid values are "
                             "meta, read, write, apps, or all.\n";
        fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, arg);
        return true;
    }
    return false;
}

/**
 * Set config file (auth file)
 * @param pOptions
 * @param arg
 * @return false on success, true on error
 */
static bool fudr_options_set_config(Fudr_Options* pOptions, const char* arg)
{
    // Nothing should be NULL
    assert(pOptions && arg);
    
    pOptions->gdrive_auth_file = malloc(strlen(arg) + 1);
    if (!pOptions->gdrive_auth_file)
    {
        // Memory error
        pOptions->error = true;
        
        // This will probably fail, but still need to try.
        pOptions->errorMsg = 
                malloc(strlen("Could not allocate memory for options\n") + 1);
        if (pOptions->errorMsg)
        {
            strcpy(pOptions->errorMsg, 
                   "Could not allocate memory for options\n");
        }
        return true;
    }
    
    strcpy(pOptions->gdrive_auth_file, arg);
    return false;
}

/**
 * Set the interaction type
 * @param pOptions
 * @param arg
 * @return false on success, true on error
 */
static bool fudr_options_set_interaction(Fudr_Options* pOptions, 
                                         const char* arg)
{
    // Nothing should be NULL
    assert(pOptions && arg);
    
    if (!strcmp(arg, "never"))
    {
        pOptions->gdrive_interaction_type = 
                GDRIVE_INTERACTION_NEVER;
    }
    else if (!strcmp(arg, "startup"))
    {
        pOptions->gdrive_interaction_type = 
                GDRIVE_INTERACTION_STARTUP;
    }
    else if (!strcmp(arg, "always"))
    {
        pOptions->gdrive_interaction_type = 
                GDRIVE_INTERACTION_ALWAYS;
    }
    else
    {
        pOptions->error = true;
        const char* fmtStr = "Unrecognized interaction type '%s'. Valid values "
                             "are always, never, and startup\n";
        fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, arg);
        return true;
    }
    return false;
}

/**
 * Set the file permissions
 * @param pOptions
 * @param arg
 * @return false on success, true on error
 */
static bool fudr_options_set_fileperm(Fudr_Options* pOptions, const char* arg)
{
    // Nothing should be NULL
    assert(pOptions && arg);
    
    char* end = NULL;
    long filePerm = strtol(arg, &end, 8);
    if (end == arg)
    {
        const char* fmtStr = "Invalid file permission '%s', not an octal "
                             "integer\n";
        fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, arg);
    }
    else if (filePerm > 0777)
    {
        const char* fmtStr = "Invalid file permission '%s', should be three "
                             "octal digits\n";
        fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, arg);
    }
    else
    {
        pOptions->file_perms = filePerm;
        return false;
    }
    pOptions->error = true;
    return true;
}

/**
 * Set directory permissions
 * @param pOptions
 * @param arg
 * @return false on success, true on error
 */
static bool fudr_options_set_dirperm(Fudr_Options* pOptions, const char* arg)
{
    // Nothing should be NULL
    assert(pOptions && arg);
    
    char* end = NULL;
    long long dirPerm = strtol(arg, &end, 8);
    if (end == arg)
    {
        const char* fmtStr = "Invalid directory permission '%s', not an octal "
                             "integer\n";
        fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, arg);
    }
    else if (dirPerm > 0777)
    {
        const char* fmtStr = "Invalid directory permission '%s', should be "
                             "three octal digits\n";
        fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, arg);
    }
    else
    {
        pOptions->dir_perms = dirPerm;
        return false;
    }
    pOptions->error = true;
    return true;
}

/**
 * Set cache TTL
 * @param pOptions
 * @param arg
 * @return false on success, true on error
 */
static bool fudr_options_set_cachettl(Fudr_Options* pOptions, const char* arg)
{
    // Nothing should be NULL
    assert(pOptions && arg);
    
    char* end = NULL;
    long cacheTime = strtol(arg, &end, 10);
    if (end == arg)
    {
        pOptions->error = true;
        const char* fmtStr = "Invalid cache-time '%s', not an integer\n";
        fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, arg);
    }
    pOptions->gdrive_cachettl = cacheTime;
    return false;
}

/**
 * Set chunk size
 * @param pOptions
 * @param arg
 * @return false on success, true on error
 */
static bool fudr_options_set_chunksize(Fudr_Options* pOptions, const char* arg)
{
    // Nothing should be NULL
    assert(pOptions && arg);
    
    char* end = NULL;
    long long chunkSize = strtoll(arg, &end, 10);
    if (end == arg)
    {
        pOptions->error = true;
        const char* fmtStr = "Invalid chunk size '%s', not an integer\n";
        fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, arg);
        return true;
    }
    pOptions->gdrive_chunk_size = chunkSize;
    return false;
}

/**
 * Set max chunks
 * @param pOptions
 * @param arg
 * @return false on success, true on error
 */
static bool fudr_options_set_maxchunks(Fudr_Options* pOptions, const char* arg)
{
    // Nothing should be NULL
    assert(pOptions && arg);
    
    char* end = NULL;
    long maxChunks = strtol(arg, &end, 10);
    if (end == arg)
    {
        pOptions->error = true;
        const char* fmtStr = "Invalid max chunks '%s', not an integer\n";
        fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, arg);
        return true;
    }
    pOptions->gdrive_max_chunks = maxChunks;
    return false;
}

/**
 * Set unrecognized option or option with required value but no value provided
 * @param pOptions
 * @param argVal:   The value of the actual argument found (as from optopt)
 * @param longIndex
 * @return   true (to be consistent with other fudr_options_set* functions that
 *          return true on error)
 */
static bool fudr_options_set_failed(Fudr_Options* pOptions, int argVal, 
                                    const struct option* longopts, 
                                    int longIndex)
{
    // Nothing should be NULL
    assert(pOptions);
    
    pOptions->error = true;
    
    // Assume short option for initialization
    char shortArgStr[] = {(char) argVal, '\0'};
    const char* argStr;
    const char* fmtStr;
    if (argVal > 32 && argVal < 256)
    {
        // Short option
        argStr = shortArgStr;
        fmtStr = "Unrecognized option, or no value given "
                         "for option: '-%s'\n";
    }
    else
    {
        // Long option
        argStr = longopts[longIndex].name;
        fmtStr = "Unrecognized option, or no value given "
                         "for option: '%s'\n";
    }
    fudr_options_make_errormsg(&pOptions->errorMsg, fmtStr, argStr);
    
    return true;
}

/**
 * Allocates and fills in an error message with one string parameter.
 * @param pDest (char**):   Address of a char array that will hold the error
 *                          message. If memory cannot be allocated, then the
 *                          pointed-to pointer will be NULL.
 * @param fmtStr (const char*): Format string as used for printf(). Must contain
 *                              exactly one "%s"
 * @param arg (const char*):    String that will be substituted into fmtStr.
 */
static void fudr_options_make_errormsg(char** pDest, const char* fmtStr, 
                                       const char* arg)
{
    // Nothing should be NULL (although the pointer at *pDest can and usually
    // should be NULL), and fmtStr should not be "".
    assert(pDest && fmtStr && fmtStr[0] && arg);
    
    int length = snprintf(NULL, 0, fmtStr, arg);
    *pDest = malloc(length + 1);
    if (*pDest)
    {
        snprintf(*pDest, length + 1, fmtStr, arg);
    }
}
