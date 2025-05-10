/************************* deviceSupportDMA.c *****************************
 *
 *  WRITTEN BY HARIS AND ANNIE
 *
 *  This file implements Phase 4 support for uMPS3 disk device DMA operations,
 *  including the synchronous disk read (SYS15) and write (SYS14) services.
 *
 *  These services allow user processes to read from or write to disk
 *  sectors by copying data between user space and kernel-reserved DMA
 *  memory buffers. Each operation translates logical block numbers into
 *  (cylinder, head, sector) format and interacts with the disk via SEEKCYL
 *  and READBLK/WRITEBLK commands, waiting for I/O completion.
 *
 *  Invalid addresses, out-of-range disk sectors, or device errors result
 *  in the termination of the requesting process.
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

/**
 * Performs a synchronous disk write operation (SYS14).
 * This function copies a 4KB buffer from the user process into a
 * kernel-reserved DMA memory frame, then issues a SEEKCYL followed by a
 * WRITEBLK command to the disk device. It waits for each I/O operation
 * to complete before proceeding. If the write fails or the parameters are
 * invalid, the process is terminated.
 *
 * @param diskNum  Disk number (0–7)
 * @param blockNum Logical 4KB block to write to (converted into cyl/head/sect)
 * @param srcAddr  Address in U-proc memory of the 4KB buffer to write
 */
void dmaWriteDisk(int diskNum, int blockNum, memaddr srcAddr)
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    if (diskNum < 0 || diskNum >= 8)
        supTerminate(); /* Invalid disk number */

    /* Resolve addresses */
    memaddr dmaAddr = RAMSTART + (DMA_DISK_START_FRAME + diskNum) * DMA_FRAME_SIZE;

    /* Copy data from user space to DMA buffer */
    char *from = (char *)srcAddr;
    char *to = (char *)dmaAddr;
    int i;
    for (i = 0; i < DISK_SECTOR_SIZE; i++)
        to[i] = from[i];

    /* Resolve device register */
    device_t *disk = (device_t *)DEV_REG_ADDR(DISKINT, diskNum);

    /* Extract disk geometry from DATA1 */
    unsigned int geometry = disk->d_data1;
    int maxCyl = (geometry >> 16) & 0xFFFF;
    int maxHead = (geometry >> 8) & 0xFF;
    int maxSect = geometry & 0xFF;

    /* Compute physical (cyl, head, sect) */
    int cyl = blockNum / (maxHead * maxSect);
    int rem = blockNum % (maxHead * maxSect);
    int head = rem / maxSect;
    int sect = rem % maxSect;

    if (cyl >= maxCyl || head >= maxHead || sect >= maxSect)
        supTerminate(); /* Out-of-bounds block number */

    /* Write DMA address to DATA0 */
    disk->d_data0 = dmaAddr;

    /* Issue SEEKCYL command */
    disk->d_command = (cyl << 8) | SEEKCYL;
    int status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    if (status != DEVICE_READY)
        supTerminate();

    /* Issue WRITEBLK command */
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

/**
 * Performs a synchronous disk read operation (SYS15).
 * This function reads a 4KB block from the specified disk location into
 * a kernel-reserved DMA buffer, then attempts to copy that data into
 * the user process’s memory at the specified address. It issues a SEEKCYL
 * followed by a READBLK command and waits for each to complete using WAITIO.
 *
 * The U-proc address must be valid and writable from kernel mode; if it is not,
 * the copy may silently fail or result in termination depending on uMPS3's memory protection.
 * If any device error or invalid parameter is encountered, the process is terminated.
 *
 * @param diskNum  Disk number (0–7)
 * @param blockNum Logical 4KB block to read from (converted into cyl/head/sect)
 * @param destAddr Address in U-proc memory where the data will be copied
 */
void dmaReadDisk(int diskNum, int blockNum, memaddr destAddr)
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    if (diskNum < 0 || diskNum >= 8)
        supTerminate(); /* Invalid disk number */

    /* Resolve DMA buffer address */
    memaddr dmaAddr = RAMSTART + (DMA_DISK_START_FRAME + diskNum) * DMA_FRAME_SIZE;

    /* Get device register */
    device_t *disk = (device_t *)DEV_REG_ADDR(DISKINT, diskNum);

    /* Extract geometry from DATA1 */
    unsigned int geometry = disk->d_data1;
    int maxCyl = (geometry >> 16) & 0xFFFF;
    int maxHead = (geometry >> 8) & 0xFF;
    int maxSect = geometry & 0xFF;

    /* Convert blockNum to (cyl, head, sect) */
    int cyl = blockNum / (maxHead * maxSect);
    int rem = blockNum % (maxHead * maxSect);
    int head = rem / maxSect;
    int sect = rem % maxSect;

    if (cyl >= maxCyl || head >= maxHead || sect >= maxSect)
        supTerminate(); /* Invalid block number */

    /* Set DATA0 to DMA buffer address */
    disk->d_data0 = dmaAddr;

    /* Seek to correct cylinder */
    disk->d_command = (cyl << 8) | SEEKCYL;
    int status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    if (status != DEVICE_READY)
        supTerminate();

    /* Issue READBLK from (head, sect) */
    disk->d_command = (head << 24) | (sect << 16) | READBLK;
    status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    if ((status & STATUS_MASK) != DEVICE_READY)
    {
        state->s_v0 = -status;
        LDST(state);
    }

    /* Copy data from DMA buffer to U-proc address */
    char *from = (char *)dmaAddr;
    char *to = (char *)destAddr;
    int i;
    for (i = 0; i < DISK_SECTOR_SIZE; i++)
        to[i] = from[i];

    state->s_v0 = DEVICE_READY;
    LDST(state);
}