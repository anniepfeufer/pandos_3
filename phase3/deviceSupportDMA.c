/************************* deviceSupportDMA.c *****************************
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

#include "../h/deviceSupportDMA.h"
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

void dmaWriteDisk(int diskNum, int blockNum, memaddr srcAddr)
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    if (diskNum < 0 || diskNum >= 8)
        supTerminate(); /* Invalid disk number */

    /* Step 1: Resolve addresses */
    memaddr dmaAddr = RAMSTART + (DMA_DISK_START_FRAME + diskNum) * DMA_FRAME_SIZE;

    /* Step 2: Copy data from user space to DMA buffer */
    char *from = (char *)srcAddr;
    char *to = (char *)dmaAddr;
    int i;
    for (i = 0; i < DISK_SECTOR_SIZE; i++)
        to[i] = from[i];

    /* Step 3: Resolve device register */
    device_t *disk = (device_t *)DEV_REG_ADDR(DISKINT, diskNum);

    /* Step 4: Extract disk geometry from DATA1 */
    unsigned int geometry = disk->d_data1;
    int maxCyl = (geometry >> 16) & 0xFFFF;
    int maxHead = (geometry >> 8) & 0xFF;
    int maxSect = geometry & 0xFF;

    /* Step 5: Compute physical (cyl, head, sect) */
    int cyl = blockNum / (maxHead * maxSect);
    int rem = blockNum % (maxHead * maxSect);
    int head = rem / maxSect;
    int sect = rem % maxSect;

    if (cyl >= maxCyl || head >= maxHead || sect >= maxSect)
        supTerminate(); /* Out-of-bounds block number */

    /* Step 6: Write DMA address to DATA0 */
    disk->d_data0 = dmaAddr;

    /* Step 7: Issue SEEKCYL command */
    disk->d_command = (cyl << 8) | SEEKCYL;
    int status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    if (status != DEVICE_READY)
        supTerminate();

    /* Step 8: Issue WRITEBLK command */
    disk->d_command = (head << 24) | (sect << 16) | WRITEBLK;
    status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    if ((status & STATUS_MASK) != DEVICE_READY)
    {
        state->s_v0 = -status;
        LDST(state);
    }
    state->s_v0 = DEVICE_READY;
    LDST(state);
}

void dmaReadDisk(int diskNum, int blockNum, memaddr destAddr)
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    if (diskNum < 0 || diskNum >= 8)
        supTerminate(); /* Invalid disk number */

    /* Step 1: Resolve DMA buffer address */
    memaddr dmaAddr = RAMSTART + (DMA_DISK_START_FRAME + diskNum) * DMA_FRAME_SIZE;

    /* Step 2: Get device register */
    device_t *disk = (device_t *)DEV_REG_ADDR(DISKINT, diskNum);

    /* Step 3: Extract geometry from DATA1 */
    unsigned int geometry = disk->d_data1;
    int maxCyl = (geometry >> 16) & 0xFFFF;
    int maxHead = (geometry >> 8) & 0xFF;
    int maxSect = geometry & 0xFF;

    /* Step 4: Convert blockNum to (cyl, head, sect) */
    int cyl = blockNum / (maxHead * maxSect);
    int rem = blockNum % (maxHead * maxSect);
    int head = rem / maxSect;
    int sect = rem % maxSect;

    if (cyl >= maxCyl || head >= maxHead || sect >= maxSect)
        supTerminate(); /* Invalid block number */

    /* Step 5: Set DATA0 to DMA buffer address */
    disk->d_data0 = dmaAddr;

    /* Step 6: Seek to correct cylinder */
    disk->d_command = (cyl << 8) | SEEKCYL;
    int status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    if (status != DEVICE_READY)
        supTerminate();

    /* Step 7: Issue READBLK from (head, sect) */
    disk->d_command = (head << 24) | (sect << 16) | READBLK;
    status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    if ((status & STATUS_MASK) != DEVICE_READY)
    {
        state->s_v0 = -status;
        LDST(state);
    }

    /* Step 8: Copy data from DMA buffer to U-proc address */
    char *from = (char *)dmaAddr;
    char *to = (char *)destAddr;
    int i;
    for (i = 0; i < DISK_SECTOR_SIZE; i++)
        to[i] = from[i];

    state->s_v0 = DEVICE_READY;
    LDST(state);
}