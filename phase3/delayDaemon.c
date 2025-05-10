/************************* delayDaemon.c *****************************
 *
 *  WRITTEN BY HARIS AND ANNIE
 *
 *  This file implements the delay facility for SYS18, allowing user processes to sleep for a specified time.
 *  Uses a statically allocated array of delay descriptors managed through a free list and a singly
 *  linked Active Delay List (ADL), which ends with a dummy tail node for simpler traversal and insertion.
 *  A kernel-mode Delay Daemon (ASID 0) runs in an infinite loop, waking every 100ms via SYS7 to check
 *  for expired delays. It wakes processes by performing a V on their private semaphores and recycles
 *  their descriptors back to the free list.
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

int ADLsem = 1;                        /* Semaphore for mutual exclusion over the ADL */
delayd_t delaydTable[DELAY_LIST_SIZE]; /* Static pool of descriptors */
delayd_t *delayd_h = NULL;             /* Head of ADL */
delayd_t *delaydFree_h = NULL;         /* Head of Free List */

/**
 * Implements the SYS18 Delay system call.
 * Validates the delay duration, allocates a delay descriptor from the free list,
 * and inserts the calling U-proc into the ADL, ordered by wake time.
 * The U-proc is then blocked on its private semaphore and resumed later by the delay daemon.
 */
void supDelay(int secCnt)
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0); /* Get support struct for current process */
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];         /* Access saved state at exception time */

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
    STCK(currTime);                                  /* Get current time */
    node->d_wakeTime = currTime + (secCnt * SECOND); /* Compute when process should resume */
    node->d_supStruct = support;                     /* Link this node to the current process */

    /* Insert into Active Delay List (sorted by wakeTime) */
    delayd_t **ptr = &delayd_h;
    while ((*ptr)->d_wakeTime < node->d_wakeTime)
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
 * Main loop for the Delay Daemon process.
 * Waits for pseudo-clock tick events, then checks the ADL.
 * If any processes have expired delay times, it wakes them by performing a V on their private semaphores,
 * and returns their delay descriptor to the free list.
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

        /* Traverse the ADL and process expired delay nodes */
        while (curr != NULL && curr->d_supStruct != NULL && curr->d_wakeTime <= currTime)
        {
            /* Wake the U-proc by V on its private semaphore */
            SYSCALL(VERHOGEN, (int)&(curr->d_supStruct->sup_privateSem), 0, 0); /* SYS4 */

            /* Remove from ADL and return to free list */
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
 * Initializes the ADL and the delay descriptor free list.
 * All usable delay descriptors (except the last) are added to the free list.
 * The final descriptor in the static pool is reserved as a dummy tail node
 * and placed at the end of the ADL to simplify insertion and traversal logic.
 * Also creates and launches the Delay Daemon process, which runs in kernel mode
 * (ASID 0) with full interrupt privileges, and is responsible for waking
 * delayed U-procs based on their scheduled wake times.
 */
void initADL()
{
    /* Step 1: Move all nodes to delayFree list */
    delayd_h = NULL;
    delaydFree_h = NULL;

    int i;
    /* Last node is reserved for dummy tail */
    for (i = 0; i < DELAY_LIST_SIZE - 1; i++)
    {
        delaydTable[i].d_next = delaydFree_h;
        delaydFree_h = &delaydTable[i];
    }

    /* Set up dummy tail node */
    delayd_t *dummy = &delaydTable[DELAY_LIST_SIZE - 1];
    dummy->d_wakeTime = MAXINT; /* Will never expire */
    dummy->d_next = NULL;
    dummy->d_supStruct = NULL;
    delayd_h = dummy; /* Start ADL with dummy tail */

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
