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
    support_t *sup = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

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
    }
}

/**
 * Wrapper function for the kernel-restricted SYS2
 * this causes the executing u-proc to cease to exist
 */
void supTerminate()
{
    /* Free all swap pool entries belonging to this process */
    int i;
    for (i = 0; i < SWAP_POOL_SIZE; i++)
    {
        if (swapPool[i].occupied && swapPool[i].asid == currentProcess->p_supportStruct->sup_asid)
        {
            freeFrame(i);
        }
    }
    SYSCALL(VERHOGEN, (int)&masterSemaphore, 0, 0);     /* SYS4: V(masterSemaphore) */
    freeSupportStruct(currentProcess->p_supportStruct); /* Free the support structure */
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
    if (len <= 0 || len > MAX_LEN || (memaddr)virtAddr < KUSEG)
    {
        supTerminate();
    }

    int asid = support->sup_asid;
    int lineNum = asid - 1; /* Translate ASID to printer line number (line = asid - 1) */
    device_t *printer = (device_t *)(DEV_REG_ADDR(IL_PRINTER, lineNum));

    /* Copy string into local buffer */
    char buffer[129]; /* +1 for null terminator */
    int i;
    for (i = 0; i < len; i++)
    {
        buffer[i] = virtAddr[i]; /* Read from U-proc’s memory */
    }
    buffer[len] = '\0';

    int charsPrinted = 0;

    /* Mutual exclusion */
    SYSCALL(PASSEREN, (int)&printerSem[lineNum], 0, 0);
    
    for (i = 0; i < len; i++)
    {
        printer->d_data0 = buffer[i];

        /* Atomically COMMAND + SYS5 */
        setSTATUS(getSTATUS() & ~IECON); /* disable interrupts */
        setENTRYHI((getENTRYHI() & VPN_MASK) | (asid << ASID_SHIFT));
        printer->d_command = PRINTCHR;
        SYSCALL(WAITIO, PRNTINT, lineNum, 0);
        setSTATUS(getSTATUS() | IECON); /* re-enable interrupts */

        /* Check for success: status should be 1 (Device Ready) */
        if ((printer->d_status & STATUS_MASK) != 1)
        {
            break; /* stop if error occurs */
        }

        charsPrinted++;
    }

    SYSCALL(VERHOGEN, (int)&printerSem[lineNum], 0, 0);

    state->s_v0 = charsPrinted;
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
    if (len <= 0 || len > MAX_LEN || (memaddr)virtAddr < KUSEG)
    {
        supTerminate();
    }

    int asid = support->sup_asid;
    int lineNum = asid - 1;
    device_t *terminal = DEV_REG_ADDR(IL_TERMINAL, lineNum);

    char buffer[129];
    int i;
    for (i = 0; i < len; i++)
    {
        buffer[i] = virtAddr[i];
    }
    buffer[len] = '\0';

    SYSCALL(PASSEREN, (int)&termWriteSem[lineNum], 0, 0);

    int sent = 0;
    for (i = 0; i < len; i++)
    {
        setSTATUS(getSTATUS() & ~IECON);
        setENTRYHI((getENTRYHI() & VPN_MASK) | (asid << ASID_SHIFT));
        terminal->t_transm_command = (buffer[i] << 8) | TRANSMITCHAR;
        SYSCALL(WAITIO, TERMINT, lineNum, TRANSMIT);
        setSTATUS(getSTATUS() | IECON);             

        if ((terminal->t_transm_status & STATUS_MASK) != 5)
        {
            break;
        }

        sent++;
    }

    SYSCALL(VERHOGEN, (int)&termWriteSem[lineNum], 0, 0);
    state->s_v0 = sent;
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

    if ((memaddr)virtAddr < KUSEG)
    {
        supTerminate();
    }

    int asid = support->sup_asid;
    int lineNum = asid - 1;
    device_t *terminal = DEV_REG_ADDR(IL_TERMINAL, lineNum);

    char buffer[129];
    int count = 0;
    char ch;

    SYSCALL(PASSEREN, (int)&termReadSem[lineNum], 0, 0);

    do
    {
        setSTATUS(getSTATUS() & ~IECON);
        setENTRYHI((getENTRYHI() & VPN_MASK) | (asid << ASID_SHIFT));
        terminal->t_recv_command = RECEIVECHAR;
        SYSCALL(WAITIO, TERMINT, lineNum, RECEIVE);
        setSTATUS(getSTATUS() | IECON);

        ch = (terminal->t_recv_status >> 8) & STATUS_MASK;

        buffer[count++] = ch;
    } while (ch != '\n' && count < MAX_LEN);

    buffer[count] = '\0'; /* null-terminate just in case */

    int i;
    for (i = 0; i < count; i++)
    {
        virtAddr[i] = buffer[i]; /* copy to user space */
    }

    SYSCALL(VERHOGEN, (int)&termReadSem[lineNum], 0, 0);

    state->s_v0 = count;
}

/**
 * terminates the calling u-proc
 */
void supportProgTrapHandler()
{
    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0); /* release mutual exclusion if held */
    /* need to check if it is held */
    supTerminate();                             /* orderly termination */
}