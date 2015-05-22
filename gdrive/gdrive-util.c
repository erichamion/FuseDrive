

#include "gdrive-util.h"


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