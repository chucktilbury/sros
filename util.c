/******************************************************************************
*
*   Miscelaneous utility functions.  This module will probably expand over 
*   time to include many functions that do not actually rate a whole sub-
*   system.
*
*/
#include <stdio.h>
#include "kern.h"

/******************************************************************************
*
*   Copy memory by the largest bus object available and then clean up with
*   bytes.  Normally, this would be an assembly funciton for maximum efficency,
*   This is really just a quick fix. 
*
*/
void copy_memory(void *dest, void *src, UINT size) {

    UINT *wbufs, *wbufd;
    UCHAR *cbufs, *cbufd;
    UINT i, maxword, remainder;

    maxword = size / sizeof(UINT);
    remainder = size % sizeof(UINT);
    wbufd = (UINT *)dest;
    wbufs = (UINT *)src;

    for(i = 0; i < maxword; i++) {
        *wbufd = *wbufs;
        wbufd++;
        wbufs++;
    }

    cbufd = (UCHAR *)wbufd;
    cbufs = (UCHAR *)wbufs;
    for(i = 0; i < remainder; i++) {
        *cbufd = *cbufs;
        cbufs++;
        cbufd++;
    }
}



