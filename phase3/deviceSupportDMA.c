/************************* deviceSupportDMA.c *****************************
 *
 *  WRITTEN BY HARIS AND ANNIE
 *
 *  This file implements Phase 4 support for uMPS3 disk and flash device DMA operations,
 *  including synchronous disk read/write (SYS15, SYS14) and flash read/write (SYS16, SYS17).
 *  These services transfer 4KB blocks between user process memory and kernel-reserved DMA buffers.
 *  Disk operations involve logical block-to-(cylinder, head, sector) translation, while flash uses
 *  linear sector addressing. All I/O commands are issued via device registers using SEEKCYL and
 *  READBLK/WRITEBLK, with completion handled through WAITIO. Invalid addresses, out-of-range sectors,
 *  or device errors result in termination of the requesting user process.
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


/**
 * Performs a flash read operation (SYS16).
 * This function reads a 4KB block from the specified flash location into
 * a kernel-reserved DMA buffer, then attempts to copy that data into
 * the user process’s memory at the specified address. It issues
 * a READBLK command and waits for each to complete using WAITIO.
 *
 * The U-proc address must be valid and writable from kernel mode; if it is not,
 * the copy may silently fail or result in termination depending on uMPS3's memory protection.
 * If any device error or invalid parameter is encountered, the process is terminated.
 *
 * @param flashNum  Flash number (0–7)
 * @param blockNum Logical 4KB block to read from 
 * @param destAddr Address in U-proc memory where the data will be copied
 */
void dmaReadFlash(int flashNum, int blockNum, memaddr destAddr)
{

    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    if (flashNum < 0 || flashNum >= 8)
        supTerminate(); /* Invalid flash number */

    /* Step 1: Resolve DMA buffer address */
    memaddr dmaAddr = RAMSTART + (DMA_FLASH_START_FRAME + flashNum) * DMA_FRAME_SIZE;

    /* Step 2: Get device register */
    device_t *flash = (device_t *)DEV_REG_ADDR(FLASHINT, flashNum);

    int MAXBLOCK = flash->d_data1;
    if (blockNum < 0 || blockNum >= MAXBLOCK)
        supTerminate(); /* Invalid block number */
    
    /* Set RAM target for flash read */
    flash->d_data0 = dmaAddr;

    int status;

    /* Atomically issue read command */
    setSTATUS(getSTATUS() & ~IECON);                               /* Disable interrupts */
    flash->d_command = (blockNum << COMMAND_SHIFT) | READBLK;      /* Issue READ command to flash */
    status = SYSCALL(WAITIO, FLASHINT, flashNum, 0);               /* Wait for I/O on flash line */
    setSTATUS(getSTATUS() | IECON);                                /* Re-enable interrupts */


    if ((status & STATUS_MASK) != DEVICE_READY)
    {
        state->s_v0 = -status;
        LDST(state);
    }

    /* Copy data from DMA buffer to U-proc address */
    char *from = (char *)dmaAddr;
    char *to = (char *)destAddr;
    int i;
    for (i = 0; i < PAGESIZE; i++)
        to[i] = from[i];

    state->s_v0 = DEVICE_READY;
    LDST(state);
   
}

/**
 * Performs a flash write operation (SYS17).
 * This function copies a 4KB buffer from the user process into a
 * kernel-reserved DMA memory frame, then issues a
 * WRITEBLK command to the flash device. It waits for the I/O operation
 * to complete before proceeding. If the write fails or the parameters are
 * invalid, the process is terminated.
 *
 * @param flashNum  flash number (0–7)
 * @param blockNum Logical 4KB block to write to (converted into cyl/head/sect)
 * @param srcAddr  Address in U-proc memory of the 4KB buffer to write
 */
void dmaWriteFlash(int flashNum, int blockNum, memaddr srcAddr)
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    if (flashNum < 0 || flashNum >= 8)
        supTerminate(); /* Invalid flash number */

    /* Resolve addresses */
    memaddr dmaAddr = RAMSTART + (DMA_FLASH_START_FRAME + flashNum) * DMA_FRAME_SIZE;

    /* Copy data from user space to DMA buffer */
    char *from = (char *)srcAddr;
    char *to = (char *)dmaAddr;
    int i;
    for (i = 0; i < PAGESIZE; i++)
        to[i] = from[i];

    /* Resolve device register */
    device_t *flash = (device_t *)DEV_REG_ADDR(FLASHINT, flashNum);

    /* Write DMA address to DATA0 */
    flash->d_data0 = dmaAddr;


    int status;

    /* Atomically issue write command */
    setSTATUS(getSTATUS() & ~IECON);                               /* Disable interrupts */
    flash->d_command = (blockNum << COMMAND_SHIFT) | WRITEBLK;      /* Issue WRITE command to flash */
    status = SYSCALL(WAITIO, FLASHINT, flashNum, 0);               /* Wait for I/O on flash line */
    setSTATUS(getSTATUS() | IECON);   


    if ((status & STATUS_MASK) != DEVICE_READY)
    {
        state->s_v0 = -status;
        LDST(state);
    }
    state->s_v0 = DEVICE_READY;
    LDST(state);
}