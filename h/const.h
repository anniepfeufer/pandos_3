#ifndef CONSTS
#define CONSTS

/****************************************************************************
 *
 * This header file contains utility constants & macro definitions.
 *
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE 4096 /* page size in bytes	*/
#define WORDLEN 4     /* word size in bytes	*/

/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR 0x10000000
#define RAMBASESIZE 0x10000004
#define TODLOADDR 0x1000001C
#define INTERVALTMR 0x10000020
#define TIMESCALEADDR 0x10000024

/* utility constants */
#define TRUE 1
#define FALSE 0
#define HIDDEN static
#define EOS '\0'

#define NULL ((void *)0xFFFFFFFF)
#define MAXPROC 20        /* Maximum number of concurrent processes */
#define MAXINT 0x7FFFFFFF /* Maximum positive integer for 32-bit systems */
#define CLOCKINTERVAL 100000UL
#define SECOND 1000000

/* Status Register Bit Masks */
#define IEPBITON 0x4         /* Previous Interrupt Enable (bit 2) */
#define KUPBITON 0x8         /* Previous Kernel/User Mode (bit 3) */
#define KUPBITOFF 0xFFFFFFF7 /* Disable User Mode (clear bit 3) */
#define TEBITON 0x08000000   /* Local Timer Enable (bit 27) */
#define ALLOFF 0x0           /* Disable all bits */
#define IM 0x0000FF00        /* Interrupt Mask (bits 8-15) */
#define IECON 0x1            /* Current Interrupt Enable bit (bit 0) */
#define TIMEROFF 0xFFFFFCFF  /* timer interrupts disabled */

#define RAMTOP (*(unsigned int *)RAMBASEADDR + *(unsigned int *)RAMBASESIZE)

#define CAUSEMASK 0xFF    /* Mask to extract ExcCode from Cause register */
#define IPMASK 0x0000FF00 /* Mask to extract Interrupts Pending from Cause register */
#define IPSHIFT 8         /* IP bits start at bit 8 */
#define RESVINSTR 10      /* Reserved Instruction (RI) Exception Code */
#define CAUSEINTOFFS 10   /* ExcCode field starts at bit 10 */
#define STATUS_MASK 0xFF  /* For device status low byte */
#define EXCEPTION_CODE_SHIFT 2

/* device interrupts */
#define DISKINT 3
#define FLASHINT 4
#define NETWINT 5
#define PRNTINT 6
#define TERMINT 7

#define DEVINTNUM 5          /* interrupt lines used by devices */
#define DEVPERINT 8          /* devices per interrupt line */
#define DEVREGLEN 4          /* device register field length in bytes, and regs per dev */
#define DEVREGSIZE 16        /* device register size in bytes */
#define BITMAPADD 0x10000040 /*physical address for the device bit map */
#define MAPMASK 0x000000FF   /*mask to get just the device mapping of the bit map word */

/* device register field number for non-terminal devices */
#define STATUS 0
#define COMMAND 1
#define DATA0 2
#define DATA1 3

/* device register field number for terminal devices */
#define RECVSTATUS 0
#define RECVCOMMAND 1
#define TRANSTATUS 2
#define TRANCOMMAND 3

/* device common STATUS codes */
#define UNINSTALLED 0
#define READY 1
#define BUSY 3

/* device common COMMAND codes */
#define RESET 0
#define ACK 1
#define DEV_REG_ADDR(intLine, devNum) ((device_t *)(0x10000054 + ((intLine - 3) * 0x80) + (devNum * 0x10)))
#define INTDEVBITMAP_ADDR(intLine) ((unsigned int *)(0x10000040 + ((intLine - 3) * 0x04)))

/* Memory related constants */
#define KSEG0 0x00000000
#define KSEG1 0x20000000
#define KSEG2 0x40000000
#define KUSEG 0x80000000
#define RAMSTART 0x20000000
#define BIOSDATAPAGE 0x0FFFF000
#define PASSUPVECTOR 0x0FFFF900

/* Exceptions related constants */
#define PGFAULTEXCEPT 0
#define GENERALEXCEPT 1

/* operations */
#define MIN(A, B) ((A) < (B) ? A : B)
#define MAX(A, B) ((A) < (B) ? B : A)
#define ALIGNED(A) (((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T) ((*((cpu_t *)INTERVALTMR)) = (T) * (*((cpu_t *)TIMESCALEADDR)))

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((*((cpu_t *)TODLOADDR)) / (*((cpu_t *)TIMESCALEADDR))))

/* SYSCALL Constants*/
#define CREATEPROCESS 1
#define TERMINATEPROCESS 2
#define PASSEREN 3
#define VERHOGEN 4
#define WAITIO 5
#define GETCPUTIME 6
#define WAITCLOCK 7
#define GETSUPPORTPTR 8
#define TERMINATE 9
#define GETTOD 10
#define WRITEPRINTER 11
#define WRITETERMINAL 12
#define READTERMINAL 13
#define DISK_GET 14
#define DISK_PUT 15
#define FLASH_GET 16
#define FLASH_PUT 17
#define DELAY 18

#define PAGE_TABLE_SIZE 32
#define SWAP_POOL_SIZE 16
#define SWAP_POOL_START_FRAME 32
#define DMA_DISK_START_FRAME 16
#define DMA_FLASH_START_FRAME 24
#define DMA_FRAME_SIZE PAGESIZE
#define DISK_SECTOR_SIZE 4096 /* Each disk sector is 4KB */

#define FRAMEPOOL RAMSTART + (SWAP_POOL_START_FRAME * PAGESIZE)

#define UPROCMAX 8
#define SUPPORT_STRUCT_POOL_SIZE UPROCMAX
#define VPNSHIFT 12 /* Shift to get VPN from EntryLo */

#define VPN_BASE 0x80000000 /* Base for text/data VPNs */
#define STACK_PAGE_INDEX 31
#define STACK_PAGE_VPN 0xBFFFF000 /* Top of kuseg for SP */

/* EntryLo bit flags */
#define ENTRYLO_DIRTY (1 << 10)
#define ENTRYLO_VALID (1 << 9)
#define ENTRYLO_GLOBAL (1 << 8)

#define ASID_SHIFT 6        /* ASID bits [11:6] in EntryHi */
#define VPN_MASK 0xFFFFF000 /* Top 20 bits for VPN */

#define EXC_MOD 1  /* TLB Modification Exception */
#define EXC_TLBL 2 /* TLB Invalid (Load/Instruction fetch) */
#define EXC_TLBS 3 /* TLB Invalid (Store) */

#define SEEKCYL 2
#define READBLK 2
#define WRITEBLK 3
#define FLASH_BASE 0x100000D4
#define FLASH_SIZE 0x10 /* each device has 16 bytes of registers */
#define COMMAND_SHIFT 8

#define UPROC_START 0x800000B0
#define UPROC_STACK 0xC0000000
#define MAX_LEN 128

#define INDEX_P_BIT 0x80000000 /* Bit 31: Probe failure (P bit) */
#define INDEX_MASK 0x0000003F  /* Bits 0–5: TLB index mask */

#define PRINTCHR 2
#define TRANSMITCHAR 2
#define RECEIVECHAR 2
#define TRANSMIT 1
#define RECEIVE 1
#define DEVICE_READY 1

#endif
