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
#include "../h/initial.h"
#include "../h/initProc.h"
#include "../h/vmSupport.h"

void supportGenExceptionHandler()
{

    /* Get the support structure for the current process */
    support_t *sup = SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* Get the exception state for the General Exception (index 0) */
    state_t *exceptionState = &sup->sup_exceptState[GENERALEXCEPT];

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
}

void supportSyscallHandler(state_t *exceptionState)
{

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
void supTerminate()
{
    /* Free all swap pool entries belonging to this process */
    for (int i = 0; i < SWAP_POOL_SIZE; i++)
    {
        if (swapPool[i].occupied && swapPool[i].asid == currentProcess->p_supportStruct->sup_asid)
        {
            freeFrame(i);
        }
    }
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}

/**
 * returns the number of microseconds since last system reboot
 * in the u-proc's v0 register
 */
void supGetTOD(state_t *exceptionState)
{

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
void supWriteToPrinter()
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    char *virtAddr = (char *)state->s_a1;
    int len = state->s_a2;

    /* Validate len and address range */
    if (len <= 0 || len > MAX_LEN || (unsigned int)virtAddr >= KUSEG)
    {
        supTerminate();
        scheduler();
    }

    /* Translate ASID to printer line number (line = asid - 1) */
    int lineNum = support->sup_asid - 1;
    /* Prepare buffer to hold the string */
    char buffer[129]; /* +1 for null terminator */

    for (int i = 0; i < len; i++)
    {
        buffer[i] = virtAddr[i]; /* Read from U-proc’s memory */
    }
    buffer[len] = '\0';

    /* Send to printer using kernel syscall */
    int status = SYSCALL(WRITEPRINTER, (int)buffer, len, lineNum);

    state->s_v0 = (status == 1) ? len : -status;
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
void supWriteToTerminal()
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    char *virtAddr = (char *)state->s_a1;
    int len = state->s_a2;

    /* Validate len and address range */
    if (len <= 0 || len > MAX_LEN || (unsigned int)virtAddr >= KUSEG)
    {
        supTerminate();
        scheduler();
    }

    /* Translate ASID to printer line number (line = asid - 1) */
    int lineNum = support->sup_asid - 1;
    /* Prepare buffer to hold the string */
    char buffer[129]; /* +1 for null terminator */

    for (int i = 0; i < len; i++)
    {
        buffer[i] = virtAddr[i]; /* Read from U-proc’s memory */
    }
    buffer[len] = '\0';

    /* Send to printer using kernel syscall */
    int status = SYSCALL(WRITETERMINAL, (int)buffer, len, lineNum);

    state->s_v0 = (status == 5) ? len : -status;
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
void supReadTerminal()
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];

    char *virtAddr = (char *)state->s_a1;

    /* Validate that virtAddr is in user space */
    if ((unsigned int)virtAddr >= KUSEG)
    {
        supTerminate();
        scheduler();
    }

    int lineNum = support->sup_asid - 1;

    char buffer[129]; /* Terminal lines can have up to 128 chars (+1 for safety) */

    /* Call kernel syscall to read from terminal */
    int status = SYSCALL(READTERMINAL, (int)buffer, 128, lineNum);

    if (status == 5)
    {
        int len = 0;
        while (len < MAX_LEN && buffer[len] != '\n' && buffer[len] != '\0')
        {
            virtAddr[len] = buffer[len]; /* Copy to user space */
            len++;
        }

        /* Copy final newline/terminator if present */
        if (buffer[len] == '\n' || buffer[len] == '\0')
        {
            virtAddr[len] = buffer[len];
            len++;
        }

        state->s_v0 = len; /* Number of chars written into virtAddr */
    }
    else
    {
        state->s_v0 = -status;
    }
}

/** 
 * terminates the calling u-proc
 */
void supportProgTrapHandler()
{
    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0); /* release mutual exclusion if held */
    supTerminate();                             /* orderly termination */
    scheduler();                              
}