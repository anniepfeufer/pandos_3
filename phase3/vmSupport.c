
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

/* Global Swap Pool Table */
swapPoolEntry_t swapPool[SWAP_POOL_SIZE];
int swapPoolSem = 1;

void uTLB_RefillHandler()
{
    state_t *savedState = (state_t *)BIOSDATAPAGE; /* Get the saved exception state */

    unsigned int entryHi = savedState->s_entryHI; /* Get the EntryHI value */
    int vpn = entryHi >> VPNSHIFT;                /* Extract VPN (upper 20 bits) */

    /* Get current process’s support structure and ASID */
    support_t *support = (support_t *)currentProcess->p_supportStruct;
    int asid = support->sup_asid;

    /* Page index = vpn - (VPN_BASE >> VPNSHIFT) */
    int pageIndex = vpn - (VPN_BASE >> VPNSHIFT); /* Locate Page Table Entry */

    if (pageIndex < 0 || pageIndex >= PAGE_TABLE_SIZE)
    {
        PANIC(); /* Safety check */
    }

    /* Get the Page Table Entry */
    pageTableEntry_t entry = support->sup_pageTable[pageIndex];

    /* Load Page Table entry into the TLB */
    setENTRYHI(entry.entryHi);
    setENTRYLO(entry.entryLo);
    TLBWR(); /* Write the entry into the TLB */

    /* Retry the instruction that caused the fault */
    LDST(savedState);
}

/* Initialize the Swap Pool Table */
    void initSwapPool()
    {
        for (int i = 0; i < SWAP_POOL_SIZE; i++)
        {
            swapPool[i].occupied = 0;
            swapPool[i].asid = -1;
            swapPool[i].vpn = -1;
        }
}

/* Find a free frame in the swap pool, returns index or -1 if full */
int getFreeFrame()
{
    for (int i = 0; i < SWAP_POOL_SIZE; i++)
    {
        if (!swapPool[i].occupied)
        {
            return i;
        }
    }
    return -1;
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
    for (int i = 0; i < SWAP_POOL_SIZE; i++)
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

    while (TRUE)
    {
        /* Wait for a page fault exception */
        STST(&(supportStruct->sup_exceptState[PGFAULTEXCEPT]));
        int badVAddr = supportStruct->sup_exceptState[PGFAULTEXCEPT].s_entryHI;
        int vpn = (badVAddr & 0xFFFFF000) >> 12;

        /* Handle the page fault */
        handlePageFault(asid, vpn);
    }
}

/* Handle the page fault logic */
void handlePageFault(int asid, int vpn)
{
    int frameIndex;

    SYSCALL(PASSEREN, (int)&swapPoolSem, 0, 0);

    /* Try to find a free frame */
    frameIndex = getFreeFrame();
    if (frameIndex == -1)
    {
        /* No free frame, pick one to evict (naive replacement) */
        frameIndex = 0; /* Simple strategy: evict first occupied frame */
        int victimASID = swapPool[frameIndex].asid;
        int victimVPN = swapPool[frameIndex].vpn;

        /* Write back the evicted page */
        writePageToBackingStore(victimASID, victimVPN, frameIndex);
    }

    /* Load the needed page */
    loadPageFromBackingStore(asid, vpn, frameIndex);

    /* Update swap pool entry */
    swapPool[frameIndex].asid = asid;
    swapPool[frameIndex].vpn = vpn;
    swapPool[frameIndex].occupied = 1;

    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);
}

void loadPageFromBackingStore(int asid, int vpn, int frame)
{
    /* Logic to read a page from backing store into frame */
}

void writePageToBackingStore(int asid, int vpn, int frame)
{
    /* Logic to write a page from frame to backing store */
}
