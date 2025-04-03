#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "../h/types.h"

/* Global Swap Pool Table and Semaphore */
extern swapPoolEntry_t swapPool[SWAP_POOL_SIZE];
extern int swapPoolSem;

/* Initializes the swap pool data structures */
extern void initSwapPool();

/* Pager exception handler for handling page faults */
extern void pagerHandler();

/* Gets the index of a free frame in the swap pool */
extern int getFreeFrame();

extern int pickVictimFrame();

/* Frees a previously used frame in the swap pool */
extern void freeFrame(int frameIndex);

/* Returns the frame index where a page is currently loaded, or -1 if not found */
extern int findFrame(int asid, int vpn);

/* Loads a page from backing store into the specified frame */
extern void loadPageFromBackingStore(int asid, int vpn, int frame);

/* Writes a page to backing store from the specified frame */
extern void writePageToBackingStore(int asid, int vpn, int frame);

support_t *getSupportStruct(int asid);

#endif 
