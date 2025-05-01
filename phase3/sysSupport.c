/************************** sysSupport.c ******************************
 *
 * WRITTEN BY HARIS AND ANNIE
 *
 * This file handles system calls 9 through 13, which are invoked when an exception
 * is passed up from a user process to its support-level exception handler. These syscalls
 * include Terminate (SYS9), getTOD (SYS10), writeToPrinter (SYS11), writeToTerminal (SYS12),
 * and readFromTerminal (SYS13). Each of these is implemented at the process level and runs
 * in the context of the user process’s support structure. This file ensures safe and
 * validated interaction between user memory and I/O devices, using local buffers,
 * uMPS3 I/O protocols, and mutual exclusion via semaphores to prevent race conditions.
 * Any invalid arguments, device errors, or unhandled cases result in the orderly
 * termination of the process.
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
#include "../h/delayDaemon.h"

/*
 * This function is called when a general exception occurs in a user process.
 * It identifies the type of exception from the cause register and dispatches control
 * to the appropriate handler. If the exception is a syscall, the syscall handler is invoked.
 * If it is a program trap or unsupported exception, the process is terminated.
 */
void supportGenExceptionHandler()
{
    /* Get the support structure for the current process */
    support_t *sup = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* Get the exception state for the General Exception (index 0) */
    state_t *exceptionState = &sup->sup_exceptState[GENERALEXCEPT];

    /* Extract detailed exception information */
    unsigned int causeReg = exceptionState->s_cause;
    int exceptionCode = (causeReg & CAUSEMASK) >> EXCEPTION_CODE_SHIFT;

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

/*
 * This function handles system calls made by user processes.
 * It reads the syscall number from register a0 and delegates execution to the appropriate
 * helper function. If the syscall is invalid, the process is terminated.
 */
void supportSyscallHandler(state_t *exceptionState)
{
    /* Move to the next instruction after syscall */
    exceptionState->s_pc += 4;

    /* Retrieve syscall number from register a0 */
    int syscallNumber = exceptionState->s_a0;

    switch (syscallNumber)
    {
    case TERMINATE:
        /* Terminate the calling process */
        supTerminate();
        break;
    case GETTOD:
        /* Return the time of day */
        supGetTOD(exceptionState);
        break;
    case WRITEPRINTER:
        /* Write string to printer device */
        supWriteToPrinter();
        break;
    case WRITETERMINAL:
        /* Write string to terminal device */
        supWriteToTerminal();
        break;
    case READTERMINAL:
        /* Read string from terminal */
        supReadTerminal();
        break;
    default:
        /* Invalid syscall, terminate the process */
        supTerminate();
    }
}

/*
 * This function cleanly terminates the calling user process.
 * It frees any swap pool frames owned by the process, releases the master semaphore
 * to notify completion, deallocates the support structure, and invokes the syscall
 * to terminate the process.
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

/*
 * This function returns the current time of day in microseconds since system boot.
 * The value is stored in the v0 register of the user process, and control is returned.
 */
void supGetTOD(state_t *exceptionState)
{

    cpu_t currentTOD;
    STCK(currentTOD); /* Store current TOD clock value */

    exceptionState->s_v0 = currentTOD; /* Return value in v0 register */

    LDST(exceptionState);
}

/*
 * Writes a string from user memory to the assigned printer device.
 * The string address is in a1 and its length in a2. The function validates
 * both, copies the string into a local buffer, and sends characters one by one
 * to the printer. It uses a semaphore to ensure exclusive access.
 * On success, the number of characters printed is returned in v0;
 * on failure, the process is terminated or a negative status is returned.
 */
void supWriteToPrinter()
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0); /* Get support struct */
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];         /* Get exception state */

    char *virtAddr = (char *)state->s_a1; /* Virtual address of string to print */
    int len = state->s_a2;

    /* Validate len and address range */
    if (len <= 0 || len > MAX_LEN || (memaddr)virtAddr < KUSEG)
    {
        supTerminate();
    }

    int asid = support->sup_asid;
    int lineNum = asid - 1;                                           /* Translate ASID to printer line number */
    device_t *printer = (device_t *)(DEV_REG_ADDR(PRNTINT, lineNum)); /* Access printer device register */

    /* Copy string into local buffer */
    char buffer[129]; /* +1 for null terminator */
    int i;
    for (i = 0; i < len; i++)
    {
        buffer[i] = virtAddr[i]; /* Copy string from user memory */
    }
    buffer[len] = '\0';

    int charsPrinted = 0;
    int status;

    /* Mutual exclusion */
    SYSCALL(PASSEREN, (int)&printerSem[lineNum], 0, 0);

    for (i = 0; i < len; i++)
    {
        printer->d_data0 = buffer[i]; /* Load character into printer buffer */

        /* Atomically COMMAND + SYS5 */
        setSTATUS(getSTATUS() & ~IECON); /* Disable interrupts */
        printer->d_command = PRINTCHR;   /* Send print command */
        status = SYSCALL(WAITIO, PRNTINT, lineNum, 0);
        setSTATUS(getSTATUS() | IECON); /* Re-enable interrupts */

        /* Check for success: status should be 1 (Device Ready) */
        if ((status & STATUS_MASK) != 1)
        {
            state->s_v0 = -status;                              /* Return error code */
            SYSCALL(VERHOGEN, (int)&printerSem[lineNum], 0, 0); /* Unlock device */
            LDST(state);                                        /* Return to user */
        }

        charsPrinted++;
    }

    SYSCALL(VERHOGEN, (int)&printerSem[lineNum], 0, 0); /* Unlock printer */
    state->s_v0 = charsPrinted;                         /* Return chars printed */
    LDST(state);                                        /* Resume process */
}

/*
 * Sends a string from user memory to the terminal’s transmit device.
 * The address is in a1 and the length in a2. The function checks for valid
 * input, copies the string locally, and sends characters individually
 * using the TRANSMITCHAR command. It ensures exclusive access via a semaphore.
 * On success, the number of characters sent is returned in v0;
 * otherwise, a negative status is returned or the process is terminated.
 */
void supWriteToTerminal()
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0); /* Get support struct */
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];         /* Get exception state */

    char *virtAddr = (char *)state->s_a1; /* Virtual address of string to write */
    int len = state->s_a2;

    /* Validate len and address range */
    if (len <= 0 || len > MAX_LEN || (memaddr)virtAddr < KUSEG)
    {
        supTerminate();
    }

    int asid = support->sup_asid;
    int lineNum = asid - 1;                              /* Translate ASID to printer line number */
    device_t *terminal = DEV_REG_ADDR(TERMINT, lineNum); /* Get terminal device register */

    char buffer[129];
    int i;
    for (i = 0; i < len; i++)
    {
        buffer[i] = virtAddr[i]; /* Copy string from user memory */
    }
    buffer[len] = '\0';

    SYSCALL(PASSEREN, (int)&termWriteSem[lineNum], 0, 0);

    int status;
    int sent = 0; /* Count of successfully sent characters */
    for (i = 0; i < len; i++)
    {
        terminal->t_transm_command = (buffer[i] << COMMAND_SHIFT) | TRANSMITCHAR; /* Load char and command into transmit register */

        setSTATUS(getSTATUS() & ~IECON); /* Disable interrupts */
        status = SYSCALL(WAITIO, TERMINT, lineNum, TRANSMIT);
        setSTATUS(getSTATUS() | IECON); /* Re-enable interrupts */

        /* Check for transmission error (STATUS should be 5 = char transmitted) */
        if ((status & STATUS_MASK) != 5)
        {
            state->s_v0 = -status;                                /* Return negative status */
            SYSCALL(VERHOGEN, (int)&termWriteSem[lineNum], 0, 0); /* Release semaphore */
            LDST(state);                                          /* Resume process with error result */
        }

        sent++;
    }

    SYSCALL(VERHOGEN, (int)&termWriteSem[lineNum], 0, 0); /* Release terminal semaphore */
    state->s_v0 = sent;                                   /* Return number of characters sent */
    LDST(state);                                          /* Resume execution */
}

/*
 * Reads input from the terminal’s receive device into a user-provided buffer at a1.
 * Input continues until a newline or max length is reached. The function checks the
 * buffer address, reads characters one at a time using RECEIVECHAR, and stores them
 * locally before copying into user memory. On success, v0 holds the number of
 * characters read; on error, it returns a negative status or terminates the process.
 */
void supReadTerminal()
{
    support_t *support = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0); /* Get support struct */
    state_t *state = &support->sup_exceptState[GENERALEXCEPT];         /* Get exception state */

    char *virtAddr = (char *)state->s_a1; /* Virtual address to store the received string */

    /* Validate the address is within user segment */
    if ((memaddr)virtAddr < KUSEG)
    {
        supTerminate();
    }

    int asid = support->sup_asid;
    int lineNum = asid - 1;
    device_t *terminal = DEV_REG_ADDR(TERMINT, lineNum);

    char buffer[129]; /* Local buffer to hold incoming characters */
    int count = 0;    /* Number of characters read */
    char ch;
    int status;

    SYSCALL(PASSEREN, (int)&termReadSem[lineNum], 0, 0); /* Lock terminal for reading */

    do
    {
        terminal->t_recv_command = RECEIVECHAR; /* Issue command to receive a character */

        setSTATUS(getSTATUS() & ~IECON);                     /* Disable interrupts */
        status = SYSCALL(WAITIO, TERMINT, lineNum, RECEIVE); /* Wait for character reception */
        setSTATUS(getSTATUS() | IECON);                      /* Re-enable interrupts */

        /* Check if the receive was successful (STATUS should be 5) */
        if ((status & STATUS_MASK) != 5)
        {
            state->s_v0 = -status;                               /* Negative device status */
            SYSCALL(VERHOGEN, (int)&termReadSem[lineNum], 0, 0); /* Unlock terminal */
            LDST(state);                                         /* Resume with error */
        }

        ch = (status >> COMMAND_SHIFT) & STATUS_MASK; /* Extract received character from status */
        buffer[count++] = ch;                         /* Store in local buffer */

    } while (ch != '\n' && count < MAX_LEN); /* Stop if newline or max length reached */

    buffer[count] = '\0'; /* Null-terminate just in case */

    int i;
    for (i = 0; i < count; i++)
    {
        virtAddr[i] = buffer[i]; /* Copy to user space */
    }

    SYSCALL(VERHOGEN, (int)&termReadSem[lineNum], 0, 0); /* Release read semaphore */
    state->s_v0 = count;                                 /* Return number of characters read */
    LDST(state);
}

/*
 * This function handles program trap exceptions raised by a user process,
 * such as illegal memory access or arithmetic errors. It ensures that any
 * held swap pool semaphore is released before terminating the process. This
 * prevents deadlocks in cases where the exception occurred while holding
 * shared resources.
 */
void supportProgTrapHandler()
{
    SYSCALL(VERHOGEN, (int)&swapPoolSem, 0, 0); /* release mutual exclusion if held */
    supTerminate();                             /* orderly termination */
}