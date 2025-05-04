
/************************** initProc.c ******************************
 *
 * WRITTEN BY HARIS AND ANNIE
 *
 * Initializes up to 8 user processes and sets up the necessary data structures
 * to integrate them with the virtual memory system, including mapping to the swap pool.
 * It calls vmSupport to initialize the swap pool table and associated semaphore,
 * and then allocates a PCB and support structure for each user process. For each process,
 * it initializes the page table with VPN-to-frame mappings, configures the pass up or die
 * exception contexts (TLB refill and general exceptions), and sets the initial processor
 * state for user-mode execution. After all fields are initialized, it uses SYS1
 * (CREATEPROCESS) to launch the processes into execution.
 *
 ***************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"
#include "../h/delayDaemon.h"

pcb_PTR asidProcessTable[UPROCMAX + 1];                /* Maps ASID (1–8) to U-proc PCB; index 0 unused */
int printerSem[8];                                     /* One binary semaphore per printer line */
int termReadSem[8];                                    /* One binary semaphore per terminal input line */
int termWriteSem[8];                                   /* One binary semaphore per terminal output line */
int masterSemaphore;                                   /* Used to synchronize termination of all U-procs */
support_t *supportFreeList = NULL;                     /* Linked list of available support_t structs */
support_t supportStructPool[SUPPORT_STRUCT_POOL_SIZE]; /* Static pool of support structs */

/*
 * Initializes the page table for a user process based on its ASID.
 * Each page table entry maps a virtual page number (VPN) to a physical frame,
 * though initially only the .data page is marked as valid and writable. The
 * stack page uses a predefined VPN, while all other pages are computed based
 * on a fixed base and incremented by page size. EntryHI for each page encodes
 * the VPN and ASID, while EntryLO is set to writable (dirty) and valid only
 * for the data page. This function must be called before the process is run
 * to ensure proper address translation during virtual memory operations.
 */
void initPageTable(support_t *supportStruct)
{
    int asid = supportStruct->sup_asid; /* Get the ASID associated with the process */
    int i;

    for (i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        unsigned int vpn;

        if (i == STACK_PAGE_INDEX) /* Use fixed VPN for stack page */
        {
            vpn = STACK_PAGE_VPN;
        }
        else
        {
            vpn = VPN_BASE + (i * PAGESIZE); /* Compute VPN for standard pages */
        }

        /* Encode VPN and ASID into EntryHI */
        supportStruct->sup_pageTable[i].entryHi = (vpn & VPN_MASK) | (asid << ASID_SHIFT);

        if (i == 1) /* Page 1 is the .data page — valid and writable */
        {
            supportStruct->sup_pageTable[i].entryLo = ENTRYLO_VALID | ENTRYLO_DIRTY;
        }
        else /* All other pages are writable but initially invalid */
        {
            supportStruct->sup_pageTable[i].entryLo = ENTRYLO_DIRTY;
        }
    }
}

/*
 * Initializes all user-level processes in the system, assigning each
 * an ASID from 1 to UPROCMAX. For each process, it allocates a pcb and a support
 * structure, initializes the process’s page table, and sets up exception contexts
 * for both TLB refill and general exceptions. These contexts include a stack pointer,
 * status register, and PC pointing to the appropriate handler (pager or syscall handler).
 * The process state is also configured for user mode execution with the correct entry
 * point, stack, and ASID. Finally, each process is stored in the ASID table to allow
 * lookup during exception handling and paging.
 */
void initUProcs()
{
    int i;

    /* Initialize 8 user processes */
    for (i = 1; i <= UPROCMAX; i++)
    {
        pcb_t *newProc = allocPcb(); /* Allocate a new PCB */
        if (newProc == NULL)
        {
            PANIC(); /* Failed to allocate process */
        }

        /* Allocate and assign support struct */
        support_t *support = allocSupportStruct();
        if (support == NULL)
        {
            PANIC(); /* Could not allocate support structure */
        }

        newProc->p_supportStruct = support; /* Link support struct to process */
        support->sup_asid = i;              /* Assign ASID to the process */
        support->sup_privateSem = 0;        /* Initialize private semaphore to 0 so SYS3 blocks */

        /* Initialize page table for the new U-proc */
        initPageTable(support);

        /* Add to ASID table for lookup by ASID */
        asidProcessTable[i] = newProc;

        /* ------------ Exception Contexts Setup ------------ */

        unsigned int tlbStack = RAMTOP - (2 * i - 1) * PAGESIZE; /* Stack for TLB handler */
        unsigned int genStack = RAMTOP - (2 * i) * PAGESIZE;     /* Stack for general handler */

        /* TLB Refill (Exception Type 0) */
        support->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = tlbStack;
        support->sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF | IEPBITON | IM | TEBITON;
        support->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr)pagerHandler;

        /* General Exception (Exception Type 1) */
        support->sup_exceptContext[GENERALEXCEPT].c_stackPtr = genStack;
        support->sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF | IEPBITON | IM | TEBITON;
        support->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr)supportGenExceptionHandler;

        /* Set entry point and SP for the U-proc */
        newProc->p_s.s_pc = UPROC_START;                                     /* Program counter */
        newProc->p_s.s_t9 = UPROC_START;                                     /* t9 is also set to entry point */
        newProc->p_s.s_sp = UPROC_STACK;                                     /* Stack pointer */
        newProc->p_s.s_status = ALLOFF | IEPBITON | IM | TEBITON | KUPBITON; /* User mode with enabled timer */
        newProc->p_s.s_entryHI = newProc->p_s.s_entryHI | (i << ASID_SHIFT); /* Encode ASID in EntryHI */
    }
}

/*
 * Allocates a support structure from the free list.
 * Returns NULL if no support structs are available.
 */
support_t *allocSupportStruct()
{
    if (supportFreeList == NULL)
        return NULL;

    support_t *allocated = supportFreeList;      /* Take from head of free list */
    supportFreeList = supportFreeList->sup_next; /* Advance the list */
    return allocated;
}

/*
 * Returns a support structure to the free list for future use.
 */
void freeSupportStruct(support_t *s)
{
    s->sup_next = supportFreeList; /* Link to current head */
    supportFreeList = s;           /* Make it the new head */
}

/*
 * Initializes semaphores and the swap pool for Phase 3.
 * Each user-level device (printer, terminal) gets its own semaphore.
 */
void initPhase3Resources()
{
    initSwapPool();
    swapPoolSem = 1;

    int i;
    for (i = 0; i < UPROCMAX; i++)
    {
        printerSem[i] = 1;
        termReadSem[i] = 1;
        termWriteSem[i] = 1;
    }

    initADL(); /* Set up delay daemon process */
}

/*
 * Populates the support structure free list using a static array.
 * Each entry in the pool is added to the front of the list.
 */
void initSupportStructs()
{
    int i;
    for (i = 0; i < SUPPORT_STRUCT_POOL_SIZE; i++)
    {
        supportStructPool[i].sup_next = supportFreeList;
        supportFreeList = &supportStructPool[i];
    }
}

/*
 * Starts all user-level processes and waits for them to complete.
 * Each process is created using CREATEPROCESS (SYS1) and synchronization
 * is handled using the master semaphore.
 */
void test()
{
    int i, j;

    /* Start each user process (1 through 8) */
    for (i = 1; i <= UPROCMAX; i++)
    {
        int result = SYSCALL(CREATEPROCESS, (int)&(asidProcessTable[i]->p_s), (int)(asidProcessTable[i]->p_supportStruct), 0);
        if (result < 0)
        {
            PANIC();
        }
    }

    /* Wait for all processes to signal completion */
    for (j = 1; j <= UPROCMAX; j++)
    {
        SYSCALL(PASSEREN, (int)&masterSemaphore, 0, 0); /* SYS3: wait for signal */
    }

    SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* SYS2: all done */
}
