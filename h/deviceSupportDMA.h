#ifndef DEVICE_SUPPORT_DMA_H
#define DEVICE_SUPPORT_DMA_H

/************************* deviceSupportDMA.H *****************************
 *
 *
 *  External declarations and constants for Phase 4 DMA-based disk I/O support.
 *  This header defines the interface for SYS14 (disk write), SYS15 (disk read),
 *  SYS16 (flash read), and SYS17 (flash write), allowing user-level processes to
 *  perform synchronous sector-based I/O via uMPS3 disk and flash devices using
 *  kernel-reserved DMA buffers. All operations rely on WAITIO for completion and
 *  are implemented in deviceSupportDMA.c.
 *
 */

#include "../h/types.h"

extern void dmaReadDisk(int diskNum, int blockNum, memaddr destAddr);
extern void dmaWriteDisk(int diskNum, int blockNum, memaddr srcAddr);
extern void dmaReadFlash(int flashNum, int blockNum, memaddr destAddr);
extern void dmaWriteFlash(int flashNum, int blockNum, memaddr srcAddr);

#endif