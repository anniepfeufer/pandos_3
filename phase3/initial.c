/************************** initial.c ******************************
 *
 * This file handles system initialization and starts the first user process.
 * It sets up global variables, configures exception handling, initializes
 * phase 1 data structures, and prepares device semaphores. The system timer
 * is configured, and the first process is created and scheduled.
 * Execution control is then transferred to the scheduler to manage processes.
 ***************************************************************/

#include "../h/initial.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/types.h"
#include "../h/const.h"

/* Global Variables */
int processCount = 0;                        /* Active process count */
int softBlockCount = 0;                      /* Soft-blocked process count */
pcb_t *readyQueue = NULL;                    /* Tail pointer to ready queue */
pcb_t *currentProcess = NULL;                /* Currently running process */
int deviceSemaphores[NUM_DEVICES + 1] = {0}; /* Device semaphores (extra one for pseudo-clock) */

/* Declaring the test function */
extern void test();

/**
 The `main` function serves as the starting point of the kernel, responsible for:
 * - Initializing global process management variables.
 * - Setting up the Pass Up Vector for handling TLB refills and exceptions.
 * - Initializing Phase 1 data structures (Pcbs and ASL).
 * - Initializing device semaphores for I/O synchronization.
 * - Setting up the system timer for periodic interrupts.
 * - Creating the initial user process and handing control to the scheduler.
 * - Entering an infinite loop if the scheduler returns (which should never happen).
 */
void main()
{
    /* Initialize Global Variables */
    processCount = 0;
    softBlockCount = 0;
    readyQueue = mkEmptyProcQ();
    currentProcess = NULL;

    /* Get the Pass Up Vector from BIOS Data Page */
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;

    /* Set the TLB Refill event handler */
    passupvector->tlb_refll_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refll_stackPtr = (memaddr)0x20001000;

    /* Set the Exception handler */
    passupvector->exception_handler = (memaddr)exceptionHandler;
    passupvector->exception_stackPtr = (memaddr)0x20001000;

    /* Initialize Phase 1 data structures */
    initPcbs();
    initASL();

    /* Initialize Nucleus variables */
    int i;
    for (i = 0; i < NUM_DEVICES + 1; i++)
    {
        deviceSemaphores[i] = 0;
    }

    /* Load the Interval Timer with 100 milliseconds */
    LDIT(CLOCKINTERVAL);

    /* Create Initial Process */
    createProcess();

    /* Start Scheduler */
    scheduler();

    /* If scheduler returns, panic (should never happen) */
    PANIC();
}

/**
 * Creates and initializes the first process.
 * - Allocates a pcb and sets up its processor state.
 * - Enables interrupts, local timer, and sets kernel mode.
 * - Sets stack pointer to RAMTOP.
 * - Sets program counter (PC) to `test` and also assigns it to `t9`.
 * - Places the process in the Ready Queue and increments process count.
 */
void createProcess()
{
    pcb_t *p = allocPcb(); /* Allocate a new pcb */

    if (p == NULL)
    {
        PANIC(); /* Should not happen (no available pcb) */
    }

    /* Initialize Processor State */
    p->p_s.s_status = IEPBITON | IM | TEBITON; /* Enable Interrupts, Timer */
    p->p_s.s_sp = RAMTOP;                      /* Set Stack Pointer to RAMTOP */
    p->p_s.s_pc = (memaddr)test;               /* Set Program Counter to `test` */
    p->p_s.s_t9 = (memaddr)test;               /* Assign t9 register to `test` */

    /* Initialize pcb fields */
    p->p_prnt = NULL;          /* No parent */
    p->p_child = NULL;         /* No children */
    p->p_sib_left = NULL;      /* No left sibling */
    p->p_sib_right = NULL;     /* No right sibling */
    p->p_time = 0;             /* Reset accumulated time */
    p->p_semAdd = NULL;        /* Not blocked on any semaphore */
    p->p_supportStruct = NULL; /* No support structure */

    /* Insert into Ready Queue */
    insertProcQ(&readyQueue, p);
    processCount++; /* Increment process count */
}
