#ifndef INIT_PROC_H
#define INIT_PROC_H

#include "../h/types.h"

extern pcb_PTR asidProcessTable[UPROCMAX + 1]; /* Index 1 to 8; index 0 unused */
static support_t supportStructPool[SUPPORT_STRUCT_POOL_SIZE];  /*Pool of support structures */
extern support_t *supportFreeList;                     /* Free list of support structures */

/* Initializes the Page Table for a given U-proc */
extern void initPageTable(support_t *supportStruct);

extern void initUProcs();

support_t *allocSupportStruct();
extern void freeSupportStruct(support_t *s);

extern void initSupportStructs();
extern void initPhase3Resources();

extern int printerSem[8];
extern int termReadSem[8];
extern int termWriteSem[8];
extern int masterSemaphore;

#endif