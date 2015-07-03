/* 
 * File:   cache-test.c
 * Author: me
 *
 * Created on June 15, 2015, 5:54 PM
 */

#ifdef CACHE_TEST

#include "fuse-drive.h"
#include "cache-test.h"
#include "gdrive/gdrive-cache.h"
#include "gdrive/gdrive-cache-node.h"
#include "gdrive/gdrive-fileid-cache-node.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static const char* commandfilename;
static FILE* outfile;
static bool keepGoing = true;

void handleCommand(int signum)
{
    if (signum != SIGUSR1)
        return;
    
    char line[1024] = {'\0'};
    FILE* commandfile = fopen(commandfilename, "r");
    fgets(line, 1024, commandfile);
    fclose(commandfile);
    if (line[strlen(line) - 1] == '\n')
    {
        line[strlen(line) - 1] = '\0';
    }
    if (strncmp(line, "ADD ", 4) == 0)
    {
        fprintf(outfile, "COMMAND: Adding '%s'\n", line + 4);
        gdrive_cache_get_item(line + 4, true, NULL);
    }
    else if (strncmp(line, "DEL ", 4) == 0)
    {
        fprintf(outfile, "COMMAND: Deleting '%s'\n", line + 4);
        gdrive_cache_delete_id(line + 4);
    }
    else
    {
        fprintf(outfile, "Invalid command: '%s'\n", line);
        fflush(outfile);
    }
    cachetest_print_fullcache(outfile);
}

void handleInterrupt(int signum)
{
    if (signum != SIGINT && signum != SIGTERM)
        return;
    keepGoing = false;
}

int main(int argc, char** argv) {
    if (argc < 3)
    {
        puts("Usage:");
        printf("%s <commandfile> <outputfile>\n\n", argv[0]);
        puts("After a command is placed in commandfile, send SIGUSR1");
        puts("to execute the command\n");
        puts("Supported commands:");
        puts("ADD fileid - Adds fileid to the MAIN cache");
        puts("DEL fileid - Removes fileid from the MAIN cache");
        puts("");
        return -1;
    }
    commandfilename = argv[1];
    FILE* commandfile = fopen(commandfilename, "r");
    if (!commandfile)
    {
        printf("Could not open command file '%s' for reading", argv[1]);
        return -1;
    }
    fclose(commandfile);
    outfile = fopen(argv[2], "w");
    if (!outfile)
    {
        printf("Could not open output file '%s' for reading", argv[2]);
        return -1;
    }
    
    if ((gdrive_init(GDRIVE_ACCESS_WRITE, "/home/me/.fuse-drive/.auth", 10, GDRIVE_INTERACTION_STARTUP, GDRIVE_BASE_CHUNK_SIZE * 4, 15)) != 0)
    {
        printf("Could not set up a Google Drive connection.");
        return 1;
    }
    
    struct sigaction act;
    act.sa_handler = handleCommand;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGUSR1, &act, NULL);
    
    act.sa_handler = handleInterrupt;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    
    while (keepGoing)
    {
        pause();
    }
    
    fclose(outfile);
    return 0;

}

#endif /* CACHE_TEST */
