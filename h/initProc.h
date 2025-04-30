#ifndef INIT_PROC_H
#define INIT_PROC_H

/************************* INITPROC.H ****************************
 *
 *  Externals declaration file for process initialization routines.
 *
 *  Declares functions and global structures used to set up
 *  user-level processes, page tables, exception contexts,
 *  and support structures. Provides utilities for allocating,
 *  freeing, and managing per-process resources.
 *
 */

#include "../h/types.h"

extern pcb_PTR asidProcessTable[UPROCMAX + 1];
extern support_t supportStructPool[SUPPORT_STRUCT_POOL_SIZE];
extern support_t *supportFreeList;

extern int printerSem[8];
extern int termReadSem[8];
extern int termWriteSem[8];
extern int masterSemaphore;

extern void initPageTable(support_t *supportStruct);
extern void test();
extern void initUProcs();
extern void freeSupportStruct(support_t *s);
extern void initSupportStructs();
extern void initPhase3Resources();

support_t *allocSupportStruct();

#endif