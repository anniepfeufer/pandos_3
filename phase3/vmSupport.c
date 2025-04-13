
/************************** vmSupport.c ******************************
 *
 * WRITTEN BY HARRIS AND ANNIE
 *
 * This file handles virtual memory support. It takes care of TLB
 * refill events, page faults (TLB invalid), and TLB modification
 * exceptions. This file takes care of updating the TLB, and
 * page tables of each user proc, and replacing a page in the
 * swap pool if needed. This file also initializes the swap pool
 * data structure and semaphore.
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

/* Global Swap Pool Table */
swapPoolEntry_t swapPool[SWAP_POOL_SIZE]; /* Swap Pool: Allocated in kernel memory (after user .text/.data) */
int swapPoolSem = 1;
static int swapIndex = 0;

/* Initialize the Swap Pool Table */
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

/* Find a free frame in the swap pool, returns index or -1 if full */
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

int pickVictimFrame()
{
    int victim = swapIndex;
    swapIndex = (swapIndex + 1) % SWAP_POOL_SIZE;
    return victim;
}

/* Free an occupied frame */
void freeFrame(int frameIndex)
{
    swapPool[frameIndex].occupied = 0;
    swapPool[frameIndex].asid = -1;
    swapPool[frameIndex].vpn = -1;
}

/* Check if a frame exists for (asid, vpn) */
int findFrame(int asid, int vpn)
{
    int i;
    for (i = 0; i < SWAP_POOL_SIZE; i++)
    {
        if (swapPool[i].occupied && swapPool[i].asid == asid && swapPool[i].vpn == vpn)
        {
            return i;
        }
    }
    return -1;
}

/* Pager process handler for page faults */
void pagerHandler()
{
    support_t *supportStruct = currentProcess->p_supportStruct;
    int asid = supportStruct->sup_asid;

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

    /* Step 6: Pick a frame */
    int frameIndex = getFreeFrame();
    if (frameIndex == -1) /* No free frame available */
    {
        frameIndex = pickVictimFrame();
        int victimASID = swapPool[frameIndex].asid;
        int victimVPN = swapPool[frameIndex].vpn;

        /* Step 8: Evict page from victim process */
        setSTATUS(getSTATUS() & ~IECON); /* Disable interrupts */

        support_t *victimSupport = getSupportStruct(victimASID);
        pageTableEntry_t *victimPTE = victimSupport->sup_pageTable;
        pageTableEntry_t *victimEntry = &victimPTE[victimVPN];

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
        setSTATUS(getSTATUS()); /* Re-enable interrupts */

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

    pageTableEntry_t *pte = &supportStruct->sup_pageTable[vpn];
    pte->entryLo = (frameIndex << VPNSHIFT) | ENTRYLO_VALID | ENTRYLO_DIRTY;

    /* Step 12: Refresh TLB */
    setENTRYHI(pte->entryHi);
    setENTRYLO(pte->entryLo);
    TLBWR();

    setSTATUS(getSTATUS()); /* Re-enable interrupts */

    /* Step 13: Release semaphore */
    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);

    /* Step 14: Return to process */
    LDST(exceptionState);
}

void loadPageFromBackingStore(int asid, int vpn, int frame)
{
    device_t *flashDev = (device_t *)(FLASH_BASE + (asid - 1) * FLASH_SIZE);

    /* Set RAM target for flash read */
    flashDev->d_data0 = (memaddr)(RAMSTART + (frame * PAGESIZE));

    /* Atomically issue read command */
    setSTATUS(getSTATUS() & ~IECON); /* Disable interrupts */
    flashDev->d_command = (vpn << COMMAND_SHIFT) | READBLK;
    SYSCALL(WAITIO, FLASHINT, asid - 1, 0); /* Wait for I/O on flash line */
    setSTATUS(getSTATUS() | IECON);         /* Re-enable interrupts */

    /* Check device status */
    if (flashDev->d_status != 1) /* 1 = Device Ready */
    {
        PANIC(); /* Handle as trap */
    }
}

void writePageToBackingStore(int asid, int vpn, int frame)
{
    device_t *flashDev = (device_t *)(FLASH_BASE + (asid - 1) * FLASH_SIZE);

    /* Set RAM source for flash write */
    flashDev->d_data0 = (memaddr)(RAMSTART + (frame * PAGESIZE));

    /* Atomically issue write command */
    setSTATUS(getSTATUS() & ~IECON); /* Disable interrupts */
    flashDev->d_command = (vpn << COMMAND_SHIFT) | WRITEBLK;
    SYSCALL(WAITIO, FLASHINT, asid - 1, 0); /* Wait for I/O on flash line */
    setSTATUS(getSTATUS() | IECON);         /* Re-enable interrupts */

    /* Check device status */
    if (flashDev->d_status != 1) /* 1 = Device Ready */
    {
        PANIC(); /* Handle as trap */
    }
}

support_t *getSupportStruct(int asid)
{
    return asidProcessTable[asid]->p_supportStruct;
}
