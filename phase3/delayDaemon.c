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

int delaySem[UPROCMAX + 1];            /* Maps ASID (1–8) to U-proc delay semaphore; index 0 unused */
int ADLsem = 1;                        /* Semaphore for mutual exclusion over the Active Delay List */
delayd_t delaydTable[DELAY_LIST_SIZE]; /* Static pool of descriptors */
delayd_t *delayd_h = NULL;             /* Head of Active Delay List (ADL) */
delayd_t *delaydFree_h = NULL;         /* Head of Free List */

/**
 * implements the SYS18 function, which allocates a delay_event
 * node, and blocks the uProc on a private semaphore
 */
void supDelay(int secCnt)
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    /* Step 1: Check that time is non-negative */
    if (secCnt < 0)
    {
        supTerminate();
    }

    /* Step 2: Get exclusive access to ADL */
    SYSCALL(PASSEREN, (int)&ADLsem, 0, 0);

    /* Step 3: Allocate a delay descriptor from the free list */
    delayd_t *node = delaydFree_h;
    if (node == NULL)
    {
        SYSCALL(VERHOGEN, (int)&ADLsem, 0, 0); /* Release lock before terminating */
        supTerminate();                        /* Could not allocate descriptor */
    }

    /* Remove from free list */
    delaydFree_h = delaydFree_h->d_next;

    /* Set wake time */
    cpu_t currTime;
    STCK(currTime);
    node->d_wakeTime = currTime + (secCnt * SECOND); /* SECOND = 1000000 */

    node->d_supStruct = support;

    /* Insert into Active Delay List (sorted by wakeTime) */
    delayd_t **ptr = &delayd_h;
    while (*ptr != NULL && (*ptr)->d_wakeTime <= node->d_wakeTime)
    {
        ptr = &(*ptr)->d_next;
    }
    node->d_next = *ptr;
    *ptr = node;

    /* Step 4: Atomically release ADL lock and block on private sem */
    SYSCALL(VERHOGEN, (int)&ADLsem, 0, 0);                  /* Release mutual exclusion */
    SYSCALL(PASSEREN, (int)&support->sup_privateSem, 0, 0); /* Block process */

    /* Step 5: Resume when delay expires */
    LDST(state);
}

/**
 * the infinite loop of the delayDaemon: executes a wait for clock,
 * and checks if it is time to wake any sleeping processes.
 */
void delayDaemon()
{
    while (TRUE)
    {
        /* Step 1: Wait for pseudo-clock tick (every 100ms) */
        SYSCALL(WAITCLOCK, 0, 0, 0); /* SYS7 */

        /* Step 2: Acquire mutual exclusion over the ADL */
        SYSCALL(PASSEREN, (int)&ADLsem, 0, 0); /* SYS3 */

        /* Step 3: Traverse and process expired delay nodes */
        cpu_t currTime;
        STCK(currTime);

        delayd_t *prev = NULL;
        delayd_t *curr = delayd_h;

        while (curr != NULL && curr->d_wakeTime <= currTime)
        {
            /* Wake the U-proc by V on its private semaphore */
            SYSCALL(VERHOGEN, (int)&(curr->d_supStruct->sup_privateSem), 0, 0); /* SYS4 */

            /* Step 3b: Remove from ADL and return to free list */
            delayd_t *expired = curr;
            curr = curr->d_next;

            if (prev == NULL)
            {
                delayd_h = curr; /* Node was at head */
            }
            else
            {
                prev->d_next = curr;
            }

            /* Return expired node to free list */
            expired->d_next = delaydFree_h;
            delaydFree_h = expired;
        }

        /* Step 4: Release ADL lock */
        SYSCALL(VERHOGEN, (int)&ADLsem, 0, 0); /* SYS4 */
    }
}

/**
 * called by the instantiator process, initializes the list
 * of active delays.
 */
void initADL()
{
    /* Step 1: Move all nodes to delayFree list */
    delayd_h = NULL; /* ADL initially empty (no dummy) */
    delaydFree_h = NULL;

    int i;
    for (i = 0; i < DELAY_LIST_SIZE; i++)
    {
        delaydTable[i].d_next = delaydFree_h;
        delaydFree_h = &delaydTable[i];
    }

    /* Step 2: Launch Delay Daemon */

    pcb_t *ddProc = allocPcb();
    if (ddProc == NULL)
    {
        PANIC();
    }

    /* Set kernel-mode state */
    ddProc->p_s.s_pc = (memaddr)delayDaemon;
    ddProc->p_s.s_t9 = (memaddr)delayDaemon;
    ddProc->p_s.s_sp = RAMTOP - 2 * PAGESIZE; /* Stack right below test's stack */
    ddProc->p_s.s_status = ALLOFF | IEPBITON | IM | TEBITON;
    ddProc->p_s.s_entryHI = 0; /* Kernel ASID = 0 */
    ddProc->p_supportStruct = NULL;

    /* Add to ready queue */
    insertProcQ(&readyQueue, ddProc);
}
