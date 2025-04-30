#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

/************************* SYSSUPPORT.H **************************
 *
 *  Externals declaration file for system call support functions.
 *
 *  Declares exception and syscall handlers used by user-level
 *  processes. Includes support for terminating processes,
 *  retrieving the time of day, and performing I/O operations
 *  with printers and terminals through the uMPS3 device interface.
 *
 */

#include "../h/types.h"

extern void supportGenExceptionHandler();
extern void supportSyscallHandler();
extern void supportProgTrapHandler();
extern void supTerminate();
extern void supGetTOD();
extern void supWriteToPrinter();
extern void supWriteToTerminal();
extern void supReadTerminal();

#endif