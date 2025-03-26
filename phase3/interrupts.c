/************************** interrupts.c ******************************
 *
 * Handles external interrupts and delegates processing to the appropriate handlers.
 *
 * This file manages hardware and timer interrupts by identifying the highest-priority
 * pending interrupt and processing device-specific events. It ensures proper
 * synchronization through semaphore operations and facilitates process scheduling
 * when necessary.
 ***************************************************************/

#include "../h/exceptions.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/scheduler.h"
#include "../h/initial.h"
#include "../h/interrupts.h"
#include "../h/const.h"

/**
 * Handles external interrupts by identifying the highest-priority pending interrupt
 * and delegating processing to the appropriate handler.
 * This function extracts the cause of the interrupt, determines the corresponding
 * interrupt line, and invokes the appropriate handler (e.g., for timers, devices, or
 * processor-local interrupts) before restoring execution state.
 */
void interruptHandler()
{
    /* Get the saved state from the BIOS Data Page */
    state_t *savedState = (state_t *)BIOSDATAPAGE;

    /* Determine the highest priority pending interrupt */
    int intLine = getHighestPriorityInterrupt(savedState->s_cause);

    switch (intLine)
    {
    case 0: /* Line 0 is ignored */
        break;

    case 1: /* Processor Local Timer (PLT) Interrupt (Highest Priority) */
        handlePLTInterrupt();
        break;

    case 2: /* Interval Timer Interrupt */
        handleIntervalTimerInterrupt();
        break;

    case 3: /* Disk Interrupt */
    case 4: /* Flash Interrupt */
    case 5: /* Network Interrupt */
    case 6: /* Printer Interrupt */
    case 7: /* Terminal Interrupt */
        handleDeviceInterrupt(intLine);
        break;

    default:
        PANIC(); /* This should never happen */
    }

    /* Restore the interrupted process */
    LDST(savedState);
}

/**
 * Handles PLT interrupts by reloading the timer, saving the process state, updating CPU time,
 * moving the process to the Ready Queue, and invoking the scheduler.
 */
void handlePLTInterrupt()
{
    /* Acknowledge the PLT interrupt by reloading the timer */
    setTIMER(5000); /* Load PLT with 5ms */

    /* Check if there's a current process */
    if (currentProcess != NULL)
    {
        /* Save process state */
        memcopy(&(currentProcess->p_s), (state_t *)BIOSDATAPAGE, sizeof(state_t));

        /* Update CPU time */
        updateCPUTime();

        /* Move the process to the Ready Queue */
        insertProcQ(&readyQueue, currentProcess);
    }

    /* Call the Scheduler */
    scheduler();
}

/**
 * Handles Interval Timer interrupts by reloading the timer, unblocking all processes
 * waiting on the Pseudo-clock semaphore, resetting the semaphore, and restoring execution.
 */
void handleIntervalTimerInterrupt()
{
    /* Acknowledge the Interval Timer interrupt by reloading the timer */
    LDIT(CLOCKINTERVAL); /* Reload Interval Timer with 100ms */

    /* Unblock all processes waiting on the Pseudo-clock semaphore */
    while (headBlocked(&deviceSemaphores[NUM_DEVICES]) != NULL)
    {
        pcb_t *unblockedProcess = removeBlocked(&deviceSemaphores[NUM_DEVICES]);
        if (unblockedProcess != NULL)
        {
            insertProcQ(&readyQueue, unblockedProcess); /* Move process to Ready Queue */
        }
    }

    /* Reset the Pseudo-clock semaphore to 0 */
    deviceSemaphores[NUM_DEVICES] = 0;

    /* Restore execution state (LDST to return control) */
    if (currentProcess != NULL)
    {
        state_t *savedState = (state_t *)BIOSDATAPAGE;
        LDST(savedState);
    }
    else
    {
        /* If no process is running, call the Scheduler */
        scheduler();
    }
}

/**
 * Handles device interrupts by identifying the highest-priority device, saving its status,
 * acknowledging the interrupt, unblocking any waiting process, and restoring execution.
 */
void handleDeviceInterrupt(int intLine)
{
    /* Determine which device caused the interrupt */
    int devNum = getHighestPriorityDevice(intLine); /* Find highest priority device on this line */

    /* Compute the device register address */
    device_t *deviceReg = DEV_REG_ADDR(intLine, devNum);

    /* Save the device's status register value BEFORE issuing ACK */
    unsigned int status;

    if (intLine == TERMINT)
    {
        if (deviceReg->t_transm_status & 0xFF) /* If low byte is non-zero, it's a Transmitter interrupt */
        {
            status = deviceReg->t_transm_status;
            deviceReg->t_transm_command = ACK; /* Acknowledge Transmitter */
        }
        else /* Otherwise, it's a Receiver interrupt */
        {
            status = deviceReg->t_recv_status;
            deviceReg->t_recv_command = ACK; /* Acknowledge Receiver */
        }
    }
    else
    {
        status = deviceReg->d_status;
        deviceReg->d_command = ACK; /* Acknowledge non-terminal device */
    }

    /* Compute the index in the deviceSemaphores array */
    int deviceIndex;
    if (intLine == TERMINT)
    {
        /* Terminal devices have two sub-devices (Receiver and Transmitter) */
        int isTransmitter = (status & 0xFF) ? 0 : 1; /* 0 for Transmitter, 1 for Receiver */
        deviceIndex = (4 * DEVPERINT) + (devNum * 2) + isTransmitter;
    }
    else
    {
        /* Other devices use normal indexing */
        deviceIndex = (intLine - 3) * DEVPERINT + devNum;
    }

    /* Get semaphore address */
    int *semAddr = &deviceSemaphores[deviceIndex];

    /* Always increment the semaphore first */
    (*semAddr)++;

    /* If semaphore is still <= 0, unblock a process */
    if (*semAddr <= 0)
    {
        /* Perform a V operation on the corresponding semaphore */
        pcb_t *unblockedProcess = removeBlocked(semAddr);

        if (unblockedProcess != NULL)
        {
            /* Store the device's status register value in v0 of the unblocked process */
            unblockedProcess->p_s.s_v0 = status;

            /* Decrement the soft block count since a process is being unblocked */
            softBlockCount--;

            /* Move the unblocked process to the Ready Queue */
            insertProcQ(&readyQueue, unblockedProcess);
        }

        /* If there's no current process, call the scheduler */
        if (currentProcess == NULL)
        {
            scheduler();
        }
        else
        {
            /* Return control to the Current Process */
            LDST((state_t *)BIOSDATAPAGE);
        }
    }
}

/**
 * Determines the highest-priority pending interrupt.
 * Reads Cause.IP and finds the lowest-numbered interrupt line that is active.
 */
int getHighestPriorityInterrupt(unsigned int cause)
{
    /* Extract and shift IP bits into correct position */
    unsigned int pendingInterrupts = (cause & IPMASK) >> IPSHIFT;

    /* Find the lowest active interrupt line */
    int i;
    for (i = 1; i <= 7; i++)
    {
        if (pendingInterrupts & (1 << i))
        {
            return i; /* Return first (highest priority) interrupt line */
        }
    }
    return -1; /* No interrupt found */
}

/**
 * Returns the device number of the highest-priority device with a pending interrupt
 */
int getHighestPriorityDevice(int intLine)
{
    /* Read the Interrupting Devices Bit Map for this line */
    unsigned int deviceBitmap = *INTDEVBITMAP_ADDR(intLine);

    if (deviceBitmap == 0)
    {
        return -1; /* No devices have pending interrupts */
    }

    /* Find the lowest set bit (highest priority device) */
    int devNum = 0;
    while ((deviceBitmap & 1) == 0)
    {
        deviceBitmap >>= 1;
        devNum++;
    }

    return devNum; /* Return first active device */
}