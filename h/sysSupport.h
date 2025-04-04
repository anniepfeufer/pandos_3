#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

#include "../h/types.h"

extern int printerSem[8];
extern int termReadSem[8];
extern int termWriteSem[8];

/* handles all passed-up, nonTLB exceptions */
extern void supportGenExceptionHandler();

/* handles syscalls 9 and above */
extern void supportSyscallHandler();

/* handles program traps for u proc */
extern void supportProgTrapHandler();

/* wrapper for the kernel-mode SYS2 */
extern void supTerminate();

/* number fo microseconds since last reboot */
extern void supGetTOD();

/* writes to the printer */
extern void supWriteToPrinter();

/* writes to the terminal */
extern void supWriteToTerminal();

/* reads from the terminal */
extern void supReadTerminal();

#endif