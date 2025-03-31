#include "../h/const.h"
#include "../h/types.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"

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
            vpn = VPN_BASE + (i * PAGE_SIZE);
        }

        /* EntryHi: [VPN | ASID in bits 11:6] */
        supportStruct->sup_pageTable[i].entryHi = (vpn & VPN_MASK) | (asid << ASID_SHIFT);

        /* EntryLo: D = 1, V = 0, G = 0 */
        supportStruct->sup_pageTable[i].entryLo = ENTRYLO_DIRTY;
    }
}
