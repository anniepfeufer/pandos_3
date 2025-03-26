#ifndef INTERRUPTS
#define INTERRUPTS

/************************* INTERRUPTS.H *****************************
 *
 *  The externals declaration file for the Interrupts
 *  module.
 *
 *  Implements an interrupt handler that determines the highest priority
 *  pending interrupt and dispatches it to the appropriate handler.
 *
 */

extern void interruptHandler();
extern void handlePLTInterrupt();
extern void handleIntervalTimerInterrupt();
extern void handleDeviceInterrupt(int intLine);
extern int getHighestPriorityInterrupt();
extern int getHighestPriorityDevice(int intLine);

/*******************************************************************/

#endif
