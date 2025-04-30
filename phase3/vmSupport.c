
/************************** vmSupport.c ******************************
 *
 * WRITTEN BY HARIS AND ANNIE
 *
 * This file handles virtual memory support for user processes. It deals with page faults
 * (caused by invalid TLB entries) and TLB modification exceptions. On a page fault, it identifies
 * the missing virtual page, allocates or evicts a frame from the swap pool, and either loads the
 * required page from the backing store or writes out a victim page. It also updates the page
 * tables of each user process and clears or modifies TLB entries as needed. The file initializes
 * and manages the swap pool data structure, uses a round-robin replacement policy when all frames
 * are occupied, and ensures mutual exclusion through a binary semaphore. Additionally, it provides
 * low-level routines to handle flash I/O operations for reading and writing virtual pages. All
 * components adhere to the uMPS3 memory management and I/O specifications.
 *
 ***************************************************************/

#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/initProc.h"
#include "../h/sysSupport.h"

swapPoolEntry_t swapPool[SWAP_POOL_SIZE]; /* Swap Pool: Allocated in kernel memory (after user .text/.data) */
int swapPoolSem = 1;                      /* Semaphore for mutual exclusion on the swap pool */
static int swapIndex = 0;                 /* Round-robin index for victim selection */

/*
 * Initializes all entries in the swap pool as unoccupied and resets metadata.
 * Must be called during VM system setup.
 */
void initSwapPool()
{
    int i;
    for (i = 0; i < SWAP_POOL_SIZE; i++)
    {
        swapPool[i].occupied = 0;
        swapPool[i].asid = -1;
        swapPool[i].vpn = -1;
    }
}

/*
 * Scans the swap pool for an available (unoccupied) frame.
 * Returns:
 *   Index of the free frame, or -1 if all frames are occupied.
 */
int getFreeFrame()
{
    int i;
    for (i = 0; i < SWAP_POOL_SIZE; i++)
    {
        if (!swapPool[i].occupied)
        {
            return i;
        }
    }
    return -1;
}

/*
 * Implements a round-robin page replacement strategy.
 * Returns:
 *   Index of the next victim frame to evict.
 */
int pickVictimFrame()
{
    int victim = swapIndex;
    swapIndex = (swapIndex + 1) % SWAP_POOL_SIZE;
    return victim;
}

/*
 * Frees the swap pool frame at the given index and resets metadata.
 * Parameters:
 *   frameIndex - index of the frame to free
 */
void freeFrame(int frameIndex)
{
    swapPool[frameIndex].occupied = 0;
    swapPool[frameIndex].asid = -1;
    swapPool[frameIndex].vpn = -1;
}

/*
 * Handles page faults for a process by either allocating a free frame
 * or evicting a page using a victim frame. Loads the missing page from
 * the backing store into memory and updates the page table and TLB.
 *
 * Steps:
 *   - Gets exception state and validates cause
 *   - Locks swap pool
 *   - Computes VPN and finds corresponding page index
 *   - Tries to allocate a free frame; evicts if needed
 *   - Writes evicted page to flash if necessary
 *   - Loads missing page from flash
 *   - Updates page table and TLB
 *   - Unlocks swap pool and resumes user process
 */
void pagerHandler()
{
    support_t *supportStruct = currentProcess->p_supportStruct; /* Get the support structure of the faulting process */
    int asid = supportStruct->sup_asid;                         /* Get the ASID of the current process */

    /* Step 1: Get exception state */
    state_t *exceptionState = &supportStruct->sup_exceptState[PGFAULTEXCEPT];

    /* Step 2: Determine cause */
    unsigned int cause = exceptionState->s_cause & MAPMASK;
    if (cause == EXC_MOD)
    {
        /* TLB Modification exceptions should not occur */
        SYSCALL(TERMINATEPROCESS, (int)currentProcess, 0, 0);
    }

    /* Step 4: Gain mutual exclusion over swap pool */
    SYSCALL(PASSEREN, (int)&swapPoolSem, 0, 0);

    /* Step 5: Determine missing VPN */
    unsigned int entryHi = exceptionState->s_entryHI;
    int vpn = (entryHi & VPN_MASK) >> VPNSHIFT;
    int pageIndex;

    /* Special handling for stack page */
    if ((vpn << VPNSHIFT) == STACK_PAGE_VPN)
    {
        pageIndex = STACK_PAGE_INDEX;
    }
    else
    {
        pageIndex = vpn - (VPN_BASE >> VPNSHIFT);
    }

    /* Step 6: Pick a frame */
    int frameIndex = getFreeFrame();
    if (frameIndex == -1) /* No free frame available */
    {
        frameIndex = pickVictimFrame();
        int victimASID = swapPool[frameIndex].asid;
        int victimVPN = swapPool[frameIndex].vpn;
        int victimPageIndex;

        /* Check if victim page is stack */
        if ((victimVPN << VPNSHIFT) == STACK_PAGE_VPN)
        {
            victimPageIndex = STACK_PAGE_INDEX;
        }
        else
        {
            victimPageIndex = victimVPN - (VPN_BASE >> VPNSHIFT);
        }

        /* Step 8: Evict page from victim process */
        setSTATUS(getSTATUS() & ~IECON); /* Disable interrupts */

        support_t *victimSupport = getSupportStruct(victimASID);
        pageTableEntry_t *victimPTE = victimSupport->sup_pageTable;
        pageTableEntry_t *victimEntry = &victimPTE[victimPageIndex];

        /* Invalidate the Page Table entry */
        victimEntry->entryLo &= ~ENTRYLO_VALID;

        /* Probe the TLB to check if entry is present */
        setENTRYHI(victimEntry->entryHi);
        TLBP();

        cpu_t index = getINDEX();
        if ((index & INDEX_P_BIT) == 0) /* TLB entry was found */
        {
            setINDEX(index & INDEX_MASK); /* Set correct index for TLBWI */
            setENTRYLO(victimEntry->entryLo);
            TLBWI(); /* Overwrite the entry in TLB */
        }
        setSTATUS(getSTATUS() | IECON); /* Re-enable interrupts */

        /* Save evicted page to flash */
        writePageToBackingStore(victimASID, victimVPN, frameIndex);
    }

    /* Step 9: Load new page from backing store */
    loadPageFromBackingStore(asid, vpn, frameIndex);

    /* Step 10: Update Swap Pool */
    swapPool[frameIndex].asid = asid;
    swapPool[frameIndex].vpn = vpn;
    swapPool[frameIndex].occupied = 1;

    /* Step 11: Update Page Table */
    setSTATUS(getSTATUS() & ~IECON); /* Disable interrupts */

    pageTableEntry_t *pte = &supportStruct->sup_pageTable[pageIndex];                     /* Get page table entry for current process */
    pte->entryLo = (FRAMEPOOL + (frameIndex * PAGESIZE)) | ENTRYLO_VALID | ENTRYLO_DIRTY; /* Setup entryLo */

    TLBCLR(); /* Flush entire TLB */

    setSTATUS(getSTATUS() | IECON); /* Re-enable interrupts */

    /* Step 13: Release semaphore */
    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);

    /* Step 14: Return to process */
    LDST(exceptionState);
}

/*
 * Loads a page from the flash device (backing store) into RAM.
 *
 * Parameters:
 *   asid  - Address Space ID of the process requesting the page
 *   vpn   - Virtual Page Number to be loaded
 *   frame - Frame index in swap pool to load into
 */
void loadPageFromBackingStore(int asid, int vpn, int frame)
{
    /* Get flash device for this ASID */
    device_t *flashDev = (device_t *)(FLASH_BASE + (asid - 1) * FLASH_SIZE);

    /* Set RAM target for flash read */
    flashDev->d_data0 = (memaddr)(RAMSTART + ((SWAP_POOL_START_FRAME + frame) * PAGESIZE));

    /* Compute page index from VPN */
    int pageIndex = vpn - (VPN_BASE >> VPNSHIFT);

    /* Atomically issue read command */
    setSTATUS(getSTATUS() & ~IECON);                              /* Disable interrupts */
    setENTRYHI((getENTRYHI() & VPN_MASK) | (asid << ASID_SHIFT)); /* Setup EntryHi for I/O */
    if (vpn == (STACK_PAGE_VPN >> VPNSHIFT))                      /* Stack page check */
    {
        pageIndex = STACK_PAGE_INDEX;
    }
    flashDev->d_command = (pageIndex << COMMAND_SHIFT) | READBLK; /* Issue READ command to flash */
    SYSCALL(WAITIO, FLASHINT, asid - 1, 0);                       /* Wait for I/O on flash line */
    setSTATUS(getSTATUS() | IECON);                               /* Re-enable interrupts */

    /* Check device status */
    if (flashDev->d_status != 1) /* 1 = Device Ready */
    {
        PANIC(); /* Handle as trap */
    }
}

/*
 * Writes the contents of a memory frame to the flash device (backing store).
 *
 * Parameters:
 *   asid  - Address Space ID of the process whose page is being evicted
 *   vpn   - Virtual Page Number being written out
 *   frame - Frame index in swap pool to write from
 */
void writePageToBackingStore(int asid, int vpn, int frame)
{
    /* Get flash device for ASID */
    device_t *flashDev = (device_t *)(FLASH_BASE + (asid - 1) * FLASH_SIZE);

    /* Set RAM source for flash write */
    flashDev->d_data0 = (memaddr)(RAMSTART + ((SWAP_POOL_START_FRAME + frame) * PAGESIZE));

    /* Translate VPN to flash page index */
    int pageIndex = vpn - (VPN_BASE >> VPNSHIFT);

    /* Atomically issue write command */
    setSTATUS(getSTATUS() & ~IECON);                              /* Disable interrupts */
    setENTRYHI((getENTRYHI() & VPN_MASK) | (asid << ASID_SHIFT)); /* Setup EntryHi for flash command */
    if (vpn == (STACK_PAGE_VPN >> VPNSHIFT))
    {
        pageIndex = STACK_PAGE_INDEX;
    }
    flashDev->d_command = (pageIndex << COMMAND_SHIFT) | WRITEBLK; /* Issue WRITE command */
    SYSCALL(WAITIO, FLASHINT, asid - 1, 0);                        /* Wait for I/O on flash line */
    setSTATUS(getSTATUS() | IECON);                                /* Re-enable interrupts */

    /* Check device status */
    if (flashDev->d_status != 1) /* 1 = Device Ready */
    {
        PANIC(); /* Handle as trap */
    }
}

/*
 * Helper function to retrieve a process's support structure given its ASID.
 *
 * Parameters:
 *   asid - Address Space ID of the process
 *
 * Returns:
 *   Pointer to the support_t structure of the corresponding process.
 */
support_t *getSupportStruct(int asid)
{
    return asidProcessTable[asid]->p_supportStruct;
}
