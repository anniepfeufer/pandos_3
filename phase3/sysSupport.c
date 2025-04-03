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
    exceptionState->s_pc += 4;

    /* Retrieve syscall number from register a0 */
    int syscallNumber = exceptionState->s_a0;

    switch (syscallNumber)
    {
    case TERMINATE:
        supTerminate();
        break;
    case GETTOD:
        supGetTOD(exceptionState);
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


/**
 * Wrapper function for the kernel-restricted SYS2
 * this causes the executing u-proc to cease to exist 
 */
void supTerminate(){


}

/**
 * returns the number of microseconds since last system reboot
 * in the u-proc's v0 register
 */
void supGetTOD(state_t* exceptionState){

    cpu_t currentTOD;
    STCK(currentTOD); /* Store current TOD clock value */

    exceptionState->s_v0 = currentTOD;
}


/**
 * causes the requesting u-proc to be suspended until the string
 * has been transmitted to the printer associated with the u-proc.
 *
 * the virtual address of the first char in the string is stored in
 * the u-proc's a1, and the length of the string is in a2. 
 * the number of char's actually transmitted is returned in v0
 * if opperation ends with status other than "device ready" 
 * return negative of status value
 *
 * if the virtual address is outside of the u-proc's address space
 * or string length is less than 0 or more than 128, call SYS9 (terminate)
 */
void supWriteToPrinter(){

}

/**
 * causes the requesting u-proc to be suspended until the string
 * has been transmitted to the terminal associated with the u-proc.
 *
 * the virtual address of the first char in the string is stored in
 * the u-proc's a1, and the length of the string is in a2. 
 * the number of char's actually transmitted is returned in v0
 * if opperation ends with status other than "char transmitted" 
 * return negative of status value
 *
 * if the virtual address is outside of the u-proc's address space
 * or string length is less than 0 or more than 128, call SYS9 (terminate)
 */
void supWriteToTerminal(){

}

/**
 * causes the requesting u-proc to be suspended until the string
 * has been transmitted from the terminal associated with the u-proc.
 * 
 * the virtual address of the string buffer where the data should be
 * placed is in a1
 * the number of char's actually transmitted is returned in v0
 * if opperation ends with status other than "char recieved" 
 * return negative of status value
 * 
 * if the virtual address for reading into is outside of the u-proc's 
 * address space, call SYS9 (terminate)
 */
void supReadTerminal(){

}