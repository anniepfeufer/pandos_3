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

extern void supDelay(int secCnt);
extern void delayDaemon();
extern void initADL();


#endif