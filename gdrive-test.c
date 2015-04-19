#ifdef __GDRIVE_TEST__

#include <stdio.h>
#include <stdbool.h>
#include "fuse-drive.h"
#include "gdrive.h"
#include <curl/curl.h>

int main(void)
{
    Gdrive_Info* pGdriveInfo = NULL;
    int result = 0;
    puts("Calling gdrive_init(&pGdriveInfo, GDRIVE_ACCESS_META, \"/home/me/.fuse-drive/.auth\", GDRIVE_INTERACTION_STARTUP):\n");
    result = gdrive_init(&pGdriveInfo, GDRIVE_ACCESS_META, "/home/me/.fuse-drive/.auth", GDRIVE_INTERACTION_STARTUP);
    puts("");
    printf("Result: %d (%s)\n", result, result == 0 ? "success" : "failure");
    puts("");
    puts("");
    char c[3];
    fgets(c, 3, stdin);
    

    
    
}



#endif	/*__GDRIVE_TEST__*/
