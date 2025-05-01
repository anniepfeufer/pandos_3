/************************* delayDaemon.c *****************************
 *
 *  The externals declaration file for the DelayDaemon
 *  module.
 *
 *  Implements the delay facility function which maintains the list of
 *  sleeping processes on a singly linked list as well as the Delay 
 *  Daemon, which is an infinite loop of waiting for clock, and checking 
 *  if any uProcs should be woken up. The Delay Daemon will run in 
 *  kernel-mode using the kernel ASID value (zero) with all interrupts enabled.
 *  also implements the function for SYS18
 *
*****************************************************************/

#include "../h/delayDaemon.h"
#include "../h/sysSupport.h"
#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/initial.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"



/**
 * implements the SYS18 function, which allocates a delay_event
 * node, and blocks the uProc on a private semaphore
 */
void supDelay(int secCnt)
{
    /* check that time is non-negative */
    if (secCnt < 0)
    {
        supTerminate();
    }

    /* obtain mutual exclusion over the ADL semaphore (SYS3) */

    /* allocate a delay_event node from the free list, populate, and 
    insert into active list */

    /* release mut-ex over ADL */

    /* execute SYS3 on uProc's private semaphore */

    /* LDST to the instruction after SYS18 (will not execute until unblocked) */
}



/** 
 * the infinite loop of the delayDaemon: executes a wait for clock, 
 * and checks if it is time to wake any sleeping processes.
 */
void delayDaemon()
{
    /* call wait for clock (SYS7) */

    /* obtain mutual exclusion over the ADL */

    /* go through the ADL active list and wake processes that are ready to be woken up */

        /* perform SYS4 on uProc's pricate semaphore */

        /* deallocate the delay_event node and return it to the free list */

    /* release mutex on ADL */

}


/** 
 * called by the instantiator process, initializes the list
 * of active delays. 
 */
void initADL()
{
    /* add the delay_event nodes from the static array to the free list */

    /* initialize the active list */

    /* initialize and launch (SYS1) the delay Daemon */

        /* PC set to delayDaemon function */

        /* SP is set to unused frame at the end of RAM */

        /* set to kernel mode with all interrupts enabled */

        /* EntryHi.ASID set to zero (kernel) */

        /* Null support structure */
    
}

