/************************** exceptions.c ******************************
 *
 * Handles all exception types in the system, including SYSCALLs,
 * program traps, and TLB exceptions.
 *
 * This file contains the Nucleus-level exception handler, which determines
 * the type of exception, processes it accordingly, and either passes it
 * to a user-level handler (if applicable) or terminates the faulty process.
 * It also implements the Pass Up or Die mechanism for exceptions that require
 * user-level handling and ensures correct system behavior when exceptions occur.
 ***************************************************************/

#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/scheduler.h"
#include "../h/initial.h"
#include "../h/const.h"

/**
 * The exception type is determined by examining the Cause register.
 * Based on the exception code, control is passed to the appropriate
 * handler (SYSCALL, program trap, TLB exception, or interrupt handler).
 * If an unhandled exception occurs, the system takes necessary actions
 * to terminate the process or halt execution.
 */
void exceptionHandler()
{
    /* Get the saved state from the BIOS Data Page */
    state_t *savedState = (state_t *)BIOSDATAPAGE;

    /* Extract detailed exception information */
    unsigned int causeReg = savedState->s_cause;
    int exceptionCode = (causeReg & CAUSEMASK) >> 2;

    switch (exceptionCode)
    {
    case 0: /* External Device Interrupt */
        interruptHandler();
        break;

    case 1:
    case 2:
    case 3: /* TLB Exceptions */
        TLBExceptionHandler();
        break;

    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 10:
    case 11:
    case 12: /* Program Traps */
        programTrapHandler();
        break;

    case 8: /* SYSCALL */
        /* Check if SYSCALL number is 9 or higher */
        if (savedState->s_a0 >= 9)
        {
            passUpOrDie(GENERALEXCEPT);
        }
        else
        {
            syscallHandler(savedState);
        }
        break;

    default:
        /* Undefined exception, terminate the process */
        sysTerminate(currentProcess);
        scheduler();
    }
}

/**
 * Determines the system call type based on the function number stored in
 * register a0 of the saved state. If a syscall is invoked from user mode,
 * the process is terminated. Otherwise, the appropriate syscall handler
 * (e.g., process control, synchronization, or I/O) is executed.
 */
void syscallHandler(state_t *savedState)
{
    /* Move to the next instruction after syscall */
    savedState->s_pc += 4;

    /* Check if the process is in user mode */
    if ((savedState->s_status & KUPBITON) != 0)
    {
        /* Simulate a Program Trap exception */
        passUpOrDie(GENERALEXCEPT);
    }

    /* Retrieve syscall number from register a0 */
    int syscallNumber = savedState->s_a0;

    switch (syscallNumber)
    {
    case CREATEPROCESS:
        /* Process creation */
        savedState->s_v0 = sysCreateProcess((state_t *)savedState->s_a1, (support_t *)savedState->s_a2);
        break;
    case TERMINATEPROCESS:
        /* debugVar2 = 0xBEEF; */
        sysTerminate(currentProcess);
        scheduler();
        break;
    case PASSEREN:
        /* Perform P() operation on a semaphore */
        sysPasseren((int *)savedState->s_a1);
        break;
    case VERHOGEN:
        /* Perform V() operation on a semaphore */
        sysVerhogen((int *)savedState->s_a1);
        break;
    case WAITIO:
        /* Block process waiting for I/O */
        sysWaitIO(savedState, savedState->s_a1, savedState->s_a2, savedState->s_a3);
        break;
    case GETCPUTIME:
        /* Return accumulated CPU time */
        sysGetCPUTime(savedState);
        break;
    case WAITCLOCK:
        /* Block process until next pseudo-clock tick */
        sysWaitClock();
        break;
    case GETSUPPORTPTR:
        /* Return the process's support structure */
        savedState->s_v0 = (int)sysGetSupportPTR();
        break;
    default:
        /* Invalid syscall, terminate the process */
        sysTerminate(currentProcess);
        scheduler();
    }

    LDST(savedState);
}

/**
 * Creates a new process with the provided processor state.
 * Allocates a new PCB, initializes its state, and sets its parent-child
 * relationship. The new process is then inserted into the Ready Queue
 * to be scheduled for execution.
 * Returns 0 on success, -1 if process creation fails (e.g., no available pcbs).
 */
int sysCreateProcess(state_t *statep, support_t *supportp)
{
    /* Allocate a new PCB */
    pcb_t *newProcess = allocPcb();
    if (newProcess == NULL)
    {
        return -1; /* No more free pcbs, return error */
    }

    /* Initialize process state from provided state pointer */
    memcopy(&(newProcess->p_s), statep, sizeof(state_t));
    newProcess->p_supportStruct = supportp; /* Set support structure (NULL if not provided) */

    /* Initialize other process fields */
    newProcess->p_time = 0;      /* Reset CPU time */
    newProcess->p_semAdd = NULL; /* Not blocked on any semaphore */

    /* Make it a child of the current process */
    insertChild(currentProcess, newProcess);

    /* Insert into Ready Queue */
    insertProcQ(&readyQueue, newProcess);

    processCount++;

    return 0; /* Success */
}

/**
 * Terminates a process and all its progeny recursively.
 * Recursively terminates all child processes, removes the process
 * from any associated semaphores, and cleans up the process tree.
 * If the terminated process is the current process, it is set to NULL.
 * If no processes remain, the system halts.
 */
void sysTerminate(pcb_t *p)
{
    if (p == NULL)
        return;

    /* Recursively terminate all children */
    while (!emptyChild(p))
    {
        sysTerminate(removeChild(p));
    }

    /* If the process is blocked on a semaphore */
    if (p->p_semAdd != NULL)
    {
        int *semAddr = p->p_semAdd;

        /* Check if it's NOT a device semaphore before adjusting */
        if (!(semAddr >= &deviceSemaphores[0] && semAddr <= &deviceSemaphores[NUM_DEVICES - 1]))
        {
            (*semAddr)++; /* Adjust the semaphore if it's NOT a device semaphore */
        }

        outBlocked(p);

        /* If the process was soft-blocked (waiting for I/O), decrement softBlockCount */
        if (semAddr >= &deviceSemaphores[0] && semAddr <= &deviceSemaphores[NUM_DEVICES - 1])
        {
            softBlockCount--;
        }
    }

    /* Remove process from the Ready Queue if it is in it */
    outProcQ(&readyQueue, p);

    /* If the process has a parent, detach it */
    if (p->p_prnt != NULL)
    {

        outChild(p);
    }

    /* If this is the current process, clear the pointer */
    if (p == currentProcess)
    {
        currentProcess = NULL;
    }

    /* Free the PCB */
    freePcb(p);

    /* Decrease active process count */
    if (processCount > 0)
    {
        processCount--;
    }

    /* If no more processes exist, HALT */
    if (processCount == 0)
    {
        HALT();
    }
}

/**
 * Performs the P (wait) operation on the given semaphore.
 * Decrements the semaphore value. If the resulting value is negative,
 * the calling process is blocked and placed in the semaphore's queue.
 * The scheduler is then invoked to select the next process to run.
 */
void sysPasseren(int *semaddr)
{
    /* Update CPU time */
    updateCPUTime();

    /* Decrement the semaphore */
    (*semaddr)--;

    /* If semaphore is negative, block the process */
    if (*semaddr < 0)
    {
        /* Save process state */
        memcopy(&(currentProcess->p_s), (state_t *)BIOSDATAPAGE, sizeof(state_t));

        /* Block current process and add it to the semaphore queue */
        currentProcess->p_semAdd = semaddr;

        insertBlocked(semaddr, currentProcess);

        /* Call the scheduler to select the next process */
        scheduler();
    }
}

/**
 * Performs the V (signal) operation on the given semaphore.
 * Increments the semaphore value. If the resulting value is zero or negative,
 * a blocked process (if any) is removed from the semaphore’s queue and moved
 * to the Ready Queue for execution.
 */
void sysVerhogen(int *semAddr)
{
    /* Increment the semaphore value */
    (*semAddr)++;

    /* If semaphore is still <= 0, unblock a process */
    if (*semAddr <= 0)
    {
        /* If any process is blocked on this semaphore, unblock the first one */
        pcb_t *unblockedProcess = removeBlocked(semAddr);

        if (unblockedProcess != NULL)
        {
            unblockedProcess->p_semAdd = NULL; /* Clear semaphore address */

            insertProcQ(&readyQueue, unblockedProcess); /* Move to Ready Queue */
        }
    }
}

/**
 * Transitions the current process from running to blocked:
 * performs a P opperation on the semaphore for the IO device.
 * Updates CPU time and state, insertBlocked, and calls scheduler.
 */
void sysWaitIO(state_t *savedState, int intLineNo, int devNum, int waitForTermRead)
{
    int deviceIndex;

    /* Compute the device index */
    if (intLineNo == TERMINT)
    {
        /* Terminals: Separate semaphores for Receiver (0-7) and Transmitter (8-15) */
        deviceIndex = (4 * DEVPERINT) + (devNum * 2) + waitForTermRead;
    }
    else
    {
        /* Other devices: Normal indexing */
        deviceIndex = (intLineNo - 3) * DEVPERINT + devNum;
    }

    int *semaddr = &(deviceSemaphores[deviceIndex]);

    /* increment softBlockCount */
    softBlockCount++;

    /* Perform P operation on the device semaphore (blocks if necessary) */
    sysPasseren(semaddr);

    /* Process should be blocked now; once unblocked, store device status */
    savedState->s_v0 = ((device_t *)DEV_REG_ADDR(intLineNo, devNum))->d_status;
}

/**
 * Retrieves the accumulated CPU time for the current process.
 */
void sysGetCPUTime(state_t *savedState)
{
    cpu_t currentTOD;
    STCK(currentTOD); /* Store current TOD clock value */

    /* Compute total CPU time: saved time + (current time - last recorded time) */
    savedState->s_v0 = currentProcess->p_time + (currentTOD - currentProcess->p_startTOD);
}

/**
 * Performs a P opperation on the nucleus maintained pseudoclock
 * semaphore. Blocks the current process on the ASL, calls the
 * scheduler.
 */
void sysWaitClock()
{
    /* Update CPU time */
    updateCPUTime();

    /* increment softBlockCount */
    softBlockCount++;

    /* Perform P() operation on the pseudo-clock semaphore */
    sysPasseren(&deviceSemaphores[NUM_DEVICES]);
}

/**
 * Returns the support structure pointer of the current process.
 */
void *sysGetSupportPTR()
{
    return currentProcess->p_supportStruct;
}

/**
 * Handles program traps, terminating the offending process.
 */
void programTrapHandler()
{
    passUpOrDie(GENERALEXCEPT);
}

/**
 * Handles TLB exceptions.
 */
void TLBExceptionHandler()
{
    passUpOrDie(PGFAULTEXCEPT);
}

/**
 * Updates the CPU time for the current process.
 */
void updateCPUTime()
{
    unsigned int currentTOD; /* TOD clock value */
    STCK(currentTOD);        /* Read the current TOD clock value */

    /* Update the accumulated CPU time */
    currentProcess->p_time += (currentTOD - currentProcess->p_startTOD);

    /* Reset the start time for the next time slice */
    currentProcess->p_startTOD = currentTOD;
}

/**
 * Handles Pass Up or Die mechanism for TLB exceptions, program traps, and SYSCALLs >= 9.
 * If the current process has a support structure, it passes the exception to the support level.
 * Otherwise, the process and all its progeny are terminated.
 */
void passUpOrDie(int exceptType)
{
    /* Check if the current process has a support structure */
    if (currentProcess->p_supportStruct == NULL)
    {
        /* No support structure, terminate the process */
        sysTerminate(currentProcess);
        scheduler();
    }
    else
    {
        /* Get source state */
        state_t *savedState = (state_t *)BIOSDATAPAGE;

        /* Copy the saved exception state */
        memcopy(&(currentProcess->p_supportStruct->sup_exceptState[exceptType]),
                savedState,
                sizeof(state_t));

        /* Load the exception handler's context */
        context_t *exceptContext = &(currentProcess->p_supportStruct->sup_exceptContext[exceptType]);
        LDCXT(exceptContext->c_stackPtr,
              exceptContext->c_status,
              exceptContext->c_pc);
    }
}

/**
 * Copies a block of memory from source to destination.
 * This function performs a byte-by-byte copy of `n` bytes from the
 * memory location pointed to by `src` to the memory location pointed to by `dest`.
 * It assumes that the memory regions do not overlap.
 */
void memcopy(void *dest, const void *src, unsigned int n)
{
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while (n--)
    {
        *d++ = *s++;
    }
}
