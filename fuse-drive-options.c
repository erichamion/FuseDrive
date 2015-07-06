

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>

#include "fuse-drive-options.h"

static Fudr_Options* fudr_options_default();

static char* fudr_options_get_default_auth_file();







Fudr_Options* fudr_options_create(int argc, char** argv)
{
    Fudr_Options* pOptions = fudr_options_default();
    if (pOptions)
    {
        pOptions->fuse_argv = malloc((argc + 2) * sizeof(char*));
        if (argc && !pOptions->fuse_argv)
        {
            fudr_options_free(pOptions);
            return NULL;
        }
        
        int i;
        for (i = 0; i < argc; i++)
        {
            if (!(strcmp(argv[i], "--access") && strcmp(argv[i], "-a")))
            {
                // Set Google Drive access level
                if (++i >= argc)
                {
                    fputs("'--access' or '-a' argument not followed by "
                            "an access level\n", stderr);
                }
                else if (!strcmp(argv[i], "meta"))
                    pOptions->gdrive_access = GDRIVE_ACCESS_META;
                else if (!strcmp(argv[i], "read"))
                    pOptions->gdrive_access = GDRIVE_ACCESS_READ;
                else if (!strcmp(argv[i], "write"))
                    pOptions->gdrive_access = GDRIVE_ACCESS_WRITE;
                else if (!strcmp(argv[i], "apps"))
                    pOptions->gdrive_access = GDRIVE_ACCESS_APPS;
                else if (!strcmp(argv[i], "all"))
                    pOptions->gdrive_access = GDRIVE_ACCESS_ALL;
                else
                    fprintf(stderr, "Unrecognized access level '%s'. Valid "
                            "values are meta, read, write, apps, or all.\n", 
                            argv[i]);
                continue;
            }
            else if (!(strcmp(argv[i], "--config") && strcmp(argv[i], "-c")))
            {
                // Set auth file
                if (++i >= argc)
                {
                    fputs("'--config' or '-c' argument not followed by "
                            "a file path\n", stderr);
                }
                else
                {
                    pOptions->gdrive_auth_file = malloc(strlen(argv[i]) + 1);
                    if (pOptions->gdrive_auth_file)
                        strcpy(pOptions->gdrive_auth_file, argv[i]);
                }
                continue;
            }
            else if (!(strcmp(argv[i], "--cache-time")))
            {
                // Set cache TTL
                if (++i >= argc)
                {
                    fputs("'--cache-time' argument not followed by a time\n", 
                          stderr);
                }
                else
                {
                    char* end = NULL;
                    long cacheTime = strtol(argv[i], &end, 10);
                    if (end == argv[i])
                    {
                        fprintf(stderr, "Invalid cache-time '%s', not an "
                                "integer\n", argv[i]);
                    }
                    else
                        pOptions->gdrive_cachettl = cacheTime;
                }
                continue;
            }
            else if (!(strcmp(argv[i], "--interaction") && strcmp(argv[i], "-i")))
            {
                // Set interaction type
                if (++i >= argc)
                {
                    fputs("'--interaction' or '-i' argument not followed by "
                            "an interaction type\n", stderr);
                }
                else
                {
                    if (!strcmp(argv[i], "never"))
                    {
                        pOptions->gdrive_interaction_type = 
                                GDRIVE_INTERACTION_NEVER;
                    }
                    else if (!strcmp(argv[i], "startup"))
                    {
                        pOptions->gdrive_interaction_type = 
                                GDRIVE_INTERACTION_STARTUP;
                    }
                    else if (!strcmp(argv[i], "always"))
                    {
                        pOptions->gdrive_interaction_type = 
                                GDRIVE_INTERACTION_ALWAYS;
                    }
                    else
                        fprintf(stderr, "Unrecognized interaction type '%s'. "
                                "Valid values are always, never, and startup\n", 
                                argv[i]);
                }
                continue;
            }
            else if (!(strcmp(argv[i], "--chunk-size")))
            {
                // Set chunk size
                if (++i >= argc)
                {
                    fputs("'--chunk-size' argument not followed by a chunk "
                            "size\n", stderr);
                }
                else
                {
                    char* end = NULL;
                    long long chunkSize = strtoll(argv[i], &end, 10);
                    if (end == argv[i])
                    {
                        fprintf(stderr, "Invalid chunk size '%s', not an "
                                "integer\n", argv[i]);
                    }
                    else
                        pOptions->gdrive_chunk_size = chunkSize;
                }
                continue;
            }
            else if (!(strcmp(argv[i], "--max-chunks")))
            {
                // Set max chunks
                if (++i >= argc)
                {
                    fputs("'--max-chunks' argument not followed by a number\n", 
                          stderr);
                }
                else
                {
                    char* end = NULL;
                    long long maxChunks = strtol(argv[i], &end, 10);
                    if (end == argv[i])
                    {
                        fprintf(stderr, "Invalid max chunks '%s', not an "
                                "integer\n", argv[i]);
                    }
                    else
                        pOptions->gdrive_max_chunks = maxChunks;
                }
                continue;
            }
            else if (!(strcmp(argv[i], "--file-perm") && strcmp(argv[i], "-p")))
            {
                // Set file permissions
                if (++i >= argc)
                {
                    fputs("'--file-perm' or '-p' argument not followed by "
                            "permissions\n", stderr);
                }
                else
                {
                    char* end = NULL;
                    long long filePerm = strtol(argv[i], &end, 8);
                    if (end == argv[i])
                    {
                        fprintf(stderr, "Invalid file permission '%s', not an "
                                "octal integer\n", argv[i]);
                    }
                    else if (filePerm > 0777)
                    {
                        fprintf(stderr, "Invalid file permission '%s', should"
                                " be three octal digits\n", argv[i]);
                    }
                    else
                        pOptions->file_perms = filePerm;
                }
                continue;
            }
            else if (!(strcmp(argv[i], "--dir-perm") && strcmp(argv[i], "-d")))
            {
                // Set directory permissions
                if (++i >= argc)
                {
                    fputs("'--dir-perm' or '-d' argument not followed by "
                            "permissions\n", stderr);
                }
                else
                {
                    char* end = NULL;
                    long long dirPerm = strtol(argv[i], &end, 8);
                    if (end == argv[i])
                    {
                        fprintf(stderr, "Invalid directory permission '%s', not "
                                "an octal integer\n", argv[i]);
                    }
                    else if (dirPerm > 0777)
                    {
                        fprintf(stderr, "Invalid directory permission '%s', "
                                "should be three octal digits\n", argv[i]);
                    }
                    else
                        pOptions->dir_perms = dirPerm;
                }
                continue;
            }
            else if (!strcmp(argv[i], "--"))
            {
                // Stop processing arguments
                i++;
                break;
            }
            else
            {
                // Pass on unrecognized options to FUSE
                pOptions->fuse_argv[pOptions->fuse_argc++] = argv[i];
            }
        }
        
        // If we stopped early (due to a "--" argument), pass everything else
        // on to FUSE
        for (; i < argc; i++)
        {
            pOptions->fuse_argv[pOptions->fuse_argc++] = argv[i];
        }
        
        if (pOptions->dir_perms == FUDR_DEFAULT_DIR_PERMS)
        {
            // Default directory permissions start by coping the file 
            // permissions, but anybody who has read permission also gets
            // execute permission.
            pOptions->dir_perms = pOptions->file_perms;
            unsigned int read_perms = pOptions->dir_perms & 0444;
            pOptions->dir_perms |= (read_perms >> 2);
        }
        
        // If we might need to interact with the user, need to add the
        // foreground option.
        if (pOptions->gdrive_interaction_type != GDRIVE_INTERACTION_NEVER)
            pOptions->fuse_argv[pOptions->fuse_argc++] = "-f";
        
        // Enforce single-threaded mode
        pOptions->fuse_argv[pOptions->fuse_argc++] = "-s";
    }
    
    return pOptions;
}

void fudr_options_free(Fudr_Options* pOptions)
{
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
    free(pOptions);
}





static Fudr_Options* fudr_options_default()
{
    Fudr_Options* pOptions = malloc(sizeof(Fudr_Options));
    if (!pOptions)
        return NULL;
    
    pOptions->gdrive_access = FUDR_DEFAULT_GDRIVE_ACCESS;
    pOptions->gdrive_auth_file = fudr_options_get_default_auth_file();
    pOptions->gdrive_cachettl = FUDR_DEFAULT_CACHETTL;
    pOptions->gdrive_interaction_type = FUDR_DEFAULT_INTERACTION;
    pOptions->gdrive_chunk_size = FUDR_DEFAULT_CHUNKSIZE;
    pOptions->gdrive_max_chunks = FUDR_DEFAULT_MAX_CHUNKS;
    pOptions->file_perms = FUDR_DEFAULT_FILE_PERMS;
    pOptions->dir_perms = FUDR_DEFAULT_DIR_PERMS;
    pOptions->fuse_argv = NULL;
    pOptions->fuse_argc = 0;
    
    return pOptions;
}

/*
 * Default file is ~/FUDR_DEFAULT_AUTH_RELPATH/FUDR_DEFAULT_AUTH_BASENAME (where 
 * ~ is the user's home directory).
 */
static char* fudr_options_get_default_auth_file()
{
    const char* homedir = getenv("HOME");
    if (!homedir)
        homedir = getpwuid(getuid())->pw_dir;
    
    char* auth_file = malloc(strlen(homedir) + strlen(FUDR_DEFAULT_AUTH_RELPATH)
                            + strlen(FUDR_DEFAULT_AUTH_BASENAME) + 3);
    if (auth_file)
    {
        strcpy(auth_file, homedir);
        strcat(auth_file, "/");
        strcat(auth_file, FUDR_DEFAULT_AUTH_RELPATH);
        strcat(auth_file, "/");
        strcat(auth_file, FUDR_DEFAULT_AUTH_BASENAME);
    }
    return auth_file;
}