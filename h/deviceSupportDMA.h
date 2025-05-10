#ifndef DEVICE_SUPPORT_DMA_H
#define DEVICE_SUPPORT_DMA_H

/************************* deviceSupportDMA.H *****************************
 *
 *
 *  External declarations and constants for Phase 4 DMA-based disk I/O
 *  support. This header defines the interface for SYS14 (disk write) and
 *  SYS15 (disk read) services used by user-level processes to perform
 *  sector-based synchronous I/O via uMPS3 disk devices.
 *
 */

#include "../h/types.h"

extern void dmaReadDisk(int diskNum, int blockNum, memaddr destAddr);
extern void dmaWriteDisk(int diskNum, int blockNum, memaddr srcAddr);
extern void dmaReadFlash(int flashNum, int blockNum, memaddr destAddr);
extern void dmaWriteFlash(int flashNum, int blockNum, memaddr srcAddr);

#endif