#ifndef INITIAL
#define INITIAL

/************************* INITIAL.H *****************************
 *
 *  The externals declaration file for the system initialization
 *  module.
 *
 *  Implements system initialization, process creation, and global
 *  variables required by the nucleus.
 *
 */

#include "../h/types.h"
#include <umps3/umps/libumps.h>

#define NUM_DEVICES ((4 * DEVPERINT) + (2 * DEVPERINT)) /* 48 semaphores */

/* Global Variables */
extern int processCount;                  
extern int softBlockCount;                
extern pcb_PTR readyQueue;               
extern pcb_PTR currentProcess;
extern int deviceSemaphores[NUM_DEVICES + 1];

/* Function Prototypes */
extern void main();
extern void createProcess();
extern void uTLB_RefillHandler(); 


/***************************************************************/

#endif
