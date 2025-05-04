#ifndef DELAYFDAEMON
#define DELAYFDAEMON

/************************* DELAYFDAEMON.H *****************************
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

extern int ADLsem;

extern void supDelay(int secCnt);
extern void delayDaemon();
extern void initADL();

#define DELAY_LIST_SIZE UPROCMAX

/* Active Delay List and Free List heads */
extern delayd_t *delayd_h;
extern delayd_t *delaydFree_h;

/* Delay descriptor pool */
extern delayd_t delaydTable[DELAY_LIST_SIZE];

#endif