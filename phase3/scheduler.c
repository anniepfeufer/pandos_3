/************************** scheduler.c ******************************
 *
 * This file implements the scheduler, which selects and dispatches the next
 * process to run from the ready queue. It ensures fair execution by setting
 * a time slice using the Process Local Timer (PLT). If no process is ready,
 * it handles cases such as waiting for I/O, detecting deadlock, or halting
 * the system when no processes remain. The scheduler is a key component of
 * process management, ensuring efficient multitasking and system stability.
 ***************************************************************/

#include "../h/scheduler.h"
#include "../h/initial.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/types.h"
#include "../h/const.h"

/**
 * The scheduler selects the next process to run and dispatches it.
 * If no process is ready, it handles termination, waiting, or deadlock scenarios.
 */
void scheduler()
{
    /* Select the next process to run */
    currentProcess = removeProcQ(&readyQueue);

    /* If no ready process exists, handle special cases */
    if (currentProcess == NULL)
    {
        if (processCount == 0)
        {
            HALT(); /* No active processes, system halts */
        }
        else if (softBlockCount > 0)
        {
            /* Wait for an I/O or timer interrupt */
            setSTATUS((IECON | IM) & ~TEBITON);
            WAIT();
        }
        else
        {
            PANIC(); /* Deadlock detected */
        }
    }

    /* Load the Process Local Timer (PLT) with 5 milliseconds */
    setTIMER(5000);

    /* Load the process state and execute */
    LDST(&(currentProcess->p_s));
}