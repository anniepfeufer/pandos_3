#ifndef INIT_PROC_H
#define INIT_PROC_H

#include "../h/types.h"

extern pcb_PTR asidProcessTable[UPROCMAX + 1]; /* Index 1 to 8; index 0 unused */

/* Initializes the Page Table for a given U-proc */
extern void initPageTable(support_t *supportStruct);

extern void initUProcs();

#endif 