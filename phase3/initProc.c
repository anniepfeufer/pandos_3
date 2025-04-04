
/************************** initProc.c ******************************
 *
 * WRITTEN BY HARRIS AND ANNIE
 *
 * Initializes up to 8 user processes and the data structures for these
 * processes to map to the swap pool. It calls initvmSupport to set up
 * the swap pool table and semaphore, itinitializes the page table,
 * pass up or die fields, and initial state for each process before calling
 * SYS1 to create process. 
 *
 ***************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/initial.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"
#include "../h/sysSupport.h"

pcb_PTR asidProcessTable[UPROCMAX + 1]; /* Index 1 to 8; index 0 unused */

void initPageTable(support_t *supportStruct)
{
    int asid = supportStruct->sup_asid;

    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        unsigned int vpn;

        if (i == STACK_PAGE_INDEX)
        {
            vpn = STACK_PAGE_VPN;
        }
        else
        {
            vpn = VPN_BASE + (i * PAGESIZE);
        }

        /* EntryHi: [VPN | ASID in bits 11:6] */
        supportStruct->sup_pageTable[i].entryHi = (vpn & VPN_MASK) | (asid << ASID_SHIFT);

        /* EntryLo: D = 1, V = 0, G = 0 */
        supportStruct->sup_pageTable[i].entryLo = ENTRYLO_DIRTY;
    }
}

void initUProcs()
{
    for (int i = 1; i <= UPROCMAX; i++)
    {
        pcb_PTR newProc = allocPcb();
        if (newProc == NULL)
        {
            PANIC(); /* Failed to allocate process */
        }

        /* Allocate and assign support struct */
        support_t *support = (support_t *)ALLOC();
        if (support == NULL)
        {
            PANIC(); /* Could not allocate support structure */
        }

        newProc->p_supportStruct = support;
        support->sup_asid = i;

        /* Initialize page table for the new U-proc */
        initPageTable(support);

        /* Add to ASID table for lookup by ASID */
        asidProcessTable[i] = newProc;

        /* ------------ Exception Contexts Setup ------------ */

        /* TLB Refill (Exception Type 0) */
        support->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) & (support->sup_stackTLB[499]);
        support->sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF | IEPBITON | IM | TEBITON;
        support->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr)pagerHandler;

        /* General Exception (Exception Type 1) */
        support->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) & (support->sup_stackGen[499]);
        support->sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF | IEPBITON | IM | TEBITON;
        support->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr)supportGenExceptionHandler;

        /* Set entry point and SP for the U-proc */
        newProc->p_s.s_pc = (memaddr)test;                        /* .text entry point */
        newProc->p_s.s_t9 = (memaddr)test;                        /* consistency with PC */
        newProc->p_s.s_sp = RAMTOP - (i * PAGESIZE);              /* unique stack */
        newProc->p_s.s_status = ALLOFF | IEPBITON | IM | TEBITON; /* user mode with timer */

        /* ------------ Launch U-proc using SYS1 ------------ */
        int result = SYSCALL(CREATEPROCESS, (int)&(newProc->p_s), (int)support, 0);
        if (result < 0)
        {
            PANIC(); /* SYS1 failed to create the U-proc */
        }
    }
}
