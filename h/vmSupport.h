#ifndef VMSUPPORT_H
#define VMSUPPORT_H

/************************* VMSUPPORT.H ***************************
 *
 *  Externals declaration file for virtual memory support routines.
 *
 *  Declares functions and variables related to the swap pool,
 *  paging operations, TLB handling, and page fault resolution.
 *  Includes support for loading and evicting pages, managing
 *  the swap pool, and handling TLB refill exceptions.
 *
 */

#include "../h/types.h"

extern swapPoolEntry_t swapPool[SWAP_POOL_SIZE];
extern int swapPoolSem;

extern void initSwapPool();
extern int getFreeFrame();
extern int pickVictimFrame();
extern void freeFrame(int frameIndex);
extern void pagerHandler();
extern void loadPageFromBackingStore(int asid, int vpn, int frame);
extern void writePageToBackingStore(int asid, int vpn, int frame);

support_t *getSupportStruct(int asid);

#endif
