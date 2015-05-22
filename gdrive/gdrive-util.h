/* 
 * File:   gdrive-util.h
 * Author: me
 * 
 * Utility functions that are needed by various parts of the Gdrive code but
 * don't really seem to be part of the Gdrive functionality.
 * 
 * TODO: This has already been reduced to a single function. Review the other
 * modules to see if anything else should be put here. If not, just put the 
 * remaining function back into gdrive-info.c.
 *
 * Created on May 9, 2015, 12:00 PM
 */

#ifndef GDRIVE_UTIL_H
#define	GDRIVE_UTIL_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include <stdio.h>

long _gdrive_divide_round_up(long dividend, long divisor);


// For debugging purposes
void dumpfile(FILE* fh, FILE* dest);


#ifdef	__cplusplus
}
#endif

#endif	/* GDRIVE_UTIL_H */

