/************************** sysSupport.c ******************************
* 
* WRITTEN BY HARRIS AND ANNIE
*
* This file handles syscalls 9+ from an exception that has been
* passed up to the user-proc's exception handler. The syscalls
* implemented in process level exception handler are Terminate(sys9),
* getTOD(sys10), writeToPrinter(sys11), writeToTerminal(sys12), and 
* readFromTermainal(sys13).
*
***************************************************************/

#include "../h/sysSupport.h"
#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"

void supportGenExceptionHandler(){

    support_t * sup = SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    exceptionState = sup-> sup_exceptState[GENERALEXCEPT];

    /* Extract detailed exception information */
    unsigned int causeReg = exceptionState->s_cause;
    int exceptionCode = (causeReg & CAUSEMASK) >> 2;

    switch (exceptionCode)
    {
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 10:
    case 11:
    case 12: /* Program Traps */
        supportProgTrapHandler();
        break;
    case 8: /* SYSCALL */
        supportSyscallHandler(exceptionState);
        break;
    default:
        /* Undefined exception, terminate the process */
        supTerminate();
        scheduler();
        

}

void supportSyscallHandler(state_t* exceptionState){

    /* Move to the next instruction after syscall */
    savedState->s_pc += 4;

    /* Retrieve syscall number from register a0 */
    int syscallNumber = exceptionState->s_a0;

    switch (syscallNumber)
    {
    case TERMINATE:
        supTerminate();
        break;
    case GETTOD:
        supGetTOD();
        break;
    case WRITEPRINTER:
        supWriteToPrinter();
        break;
    case WRITETERMINAL:
        supWriteToTerminal();
        break;
    case READTERMINAL:
        supReadTerminal();
        break;
    default:
        /* Invalid syscall, terminate the process */
        supTerminate();
        scheduler();
    }
    
}