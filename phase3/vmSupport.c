#include "../h/vmSupport.h"
#include "../h/initial.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/initProc.h"

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
        frameIndex = 0; /* No free frame, use the first one in the swap pool */
        int victimASID = swapPool[frameIndex].asid;
        int victimVPN = swapPool[frameIndex].vpn;

        /* Step 8: Evict page from victim process */
        support_t *victimSupport = getSupportStruct(victimASID);
        pageTableEntry_t *victimPTE = victimSupport->sup_pageTable;

        victimPTE[victimVPN].entryLo &= ~ENTRYLO_VALID; /* Invalidate the entry in the TLB */
        TLBCLR();                                       /* Clear the TLB */

        writePageToBackingStore(victimASID, victimVPN, frameIndex);
    }

    /* Step 9: Load new page */
    loadPageFromBackingStore(asid, vpn, frameIndex);

    /* Step 10: Update Swap Pool */
    swapPool[frameIndex].asid = asid;
    swapPool[frameIndex].vpn = vpn;
    swapPool[frameIndex].occupied = 1;

    /* Step 11: Update Page Table */
    pageTableEntry_t *pte = &supportStruct->sup_pageTable[vpn];
    pte->entryLo = (frameIndex << VPNSHIFT) | ENTRYLO_VALID | ENTRYLO_DIRTY;

    /* Step 12: Refresh TLB */
    setENTRYHI(pte->entryHi);
    setENTRYLO(pte->entryLo);
    TLBWR();

    /* Step 13: Release semaphore */
    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0);

    /* Step 14: Return to process */
    LDST(exceptionState);
}

void loadPageFromBackingStore(int asid, int vpn, int frame)
{
    /* Logic to read a page from backing store into frame */
}

void writePageToBackingStore(int asid, int vpn, int frame)
{
    /* Logic to write a page from frame to backing store */
}

support_t *getSupportStruct(int asid)
{
    return asidProcessTable[asid]->p_supportStruct;
}
