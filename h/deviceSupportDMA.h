#ifndef DEVICE_SUPPORT_DMA_H
#define DEVICE_SUPPORT_DMA_H

/************************* deviceSupportDMA.H *****************************
 *
 *  The externals declaration file for the DelayDaemon
 *  module.
 *
 *  Implements the delay facility function which maintains the list of
 *  sleeping processes, as well as the Delay Daemon, which is an infinite
 *  loop of waiting for clock, and checking if any uProcs should be
 *  woken up.
 *
 */

#include "../h/types.h"

extern void dmaReadDisk(int diskNum, int blockNum, memaddr destAddr);
extern void dmaWriteDisk(int diskNum, int blockNum, memaddr srcAddr);

#endif