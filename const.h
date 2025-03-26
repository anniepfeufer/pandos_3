#ifndef CONSTS
#define CONSTS

/**************************************************************************** 
 *
 * This header file contains utility constants & macro definitions.
 * 
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE		  4096			/* page size in bytes	*/
#define WORDLEN			  4				  /* word size in bytes	*/


/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		0x10000000
#define RAMBASESIZE		0x10000004
#define TODLOADDR		  0x1000001C
#define INTERVALTMR		0x10000020	
#define TIMESCALEADDR	0x10000024


/* utility constants */
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			  static
#define EOS				    '\0'

#define NULL 			    ((void *)0xFFFFFFFF)
#define MAXPROC 20          /* Maximum number of concurrent processes */
#define MAXINT 0x7FFFFFFF   /* Maximum positive integer for 32-bit systems */
#define CLOCKINTERVAL 100000UL

/* Status Register Bit Masks */
#define IEPBITON 0x4         /* Previous Interrupt Enable (bit 2) */
#define KUPBITON 0x8         /* Previous Kernel/User Mode (bit 3) */
#define KUPBITOFF 0xFFFFFFF7 /* Disable User Mode (clear bit 3) */
#define TEBITON 0x08000000   /* Local Timer Enable (bit 27) */
#define ALLOFF 0x0           /* Disable all bits */
#define IM 0x0000FF00        /* Interrupt Mask (bits 8-15) */
#define IECON 0x1            /* Current Interrupt Enable bit (bit 0) */

#define RAMTOP (*(unsigned int *)RAMBASEADDR + *(unsigned int *)RAMBASESIZE)

#define CAUSEMASK 0xFF       /* Mask to extract ExcCode from Cause register */
#define IPMASK 0x0000FF00    /* Mask to extract Interrupts Pending from Cause register */
#define IPSHIFT 8            /* IP bits start at bit 8 */
#define RESVINSTR 10         /* Reserved Instruction (RI) Exception Code */
#define CAUSEINTOFFS 10       /* ExcCode field starts at bit 10 */

/* device interrupts */
#define DISKINT			  3
#define FLASHINT 		  4
#define NETWINT 		  5
#define PRNTINT 		  6
#define TERMINT			  7

#define DEVINTNUM		  5		  /* interrupt lines used by devices */
#define DEVPERINT		  8		  /* devices per interrupt line */
#define DEVREGLEN		  4		  /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	  16 		/* device register size in bytes */
#define BITMAPADD         0x10000040 /*physical address for the device bit map */
#define MAPMASK           0x000000FF /*mask to get just the device mapping of the bit map word */

/* device register field number for non-terminal devices */
#define STATUS			  0
#define COMMAND			  1
#define DATA0			    2
#define DATA1			    3

/* device register field number for terminal devices */
#define RECVSTATUS  	0
#define RECVCOMMAND 	1
#define TRANSTATUS  	2
#define TRANCOMMAND 	3

/* device common STATUS codes */
#define UNINSTALLED		0
#define READY			    1
#define BUSY			    3

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1
#define DEV_REG_ADDR(intLine, devNum) ((device_t *)(0x10000054 + ((intLine - 3) * 0x80) + (devNum * 0x10)))
#define INTDEVBITMAP_ADDR(intLine) ((unsigned int *)(0x10000040 + ((intLine - 3) * 0x04)))

/* Memory related constants */
#define KSEG0           0x00000000
#define KSEG1           0x20000000
#define KSEG2           0x40000000
#define KUSEG           0x80000000
#define RAMSTART        0x20000000
#define BIOSDATAPAGE    0x0FFFF000
#define	PASSUPVECTOR	  0x0FFFF900

/* Exceptions related constants */
#define	PGFAULTEXCEPT	  0
#define GENERALEXCEPT	  1


/* operations */
#define	MIN(A,B)		((A) < (B) ? A : B)
#define MAX(A,B)		((A) < (B) ? B : A)
#define	ALIGNED(A)		(((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

/* SYSCALL Constants*/
#define CREATEPROCESS      1
#define TERMINATEPROCESS   2
#define PASSEREN           3
#define VERHOGEN           4
#define WAITIO             5
#define GETCPUTIME         6
#define WAITCLOCK          7
#define GETSUPPORTPTR      8

#endif
