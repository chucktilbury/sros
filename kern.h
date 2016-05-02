/*****************************************************************************\
*
*   Main system include file.  Makes sure that aoo of the other includes are 
*   included in the correct order.
*
\*****************************************************************************/
#ifndef __KERN_HEADER_DEFINED__
#define __KERN_HEADER_DEFINED__

/******************************************************************************
*   Configuration constants
*
*   Make one of the following definitions active to select which system to.
*   You could use the command line in the make file or else copy one of them 
*   out of the comment to make it active.
*
#define ANYOS
#define X86
#define LINUX
#define STAND_ALONE
*/

#if defined(LINUX)
#   include "linux/system.h"
#   define _USE_SETJMP_
#endif

#if defined(ANYOS)
#   include "anyos/system.h"
#   define _USE_SETJMP_
#endif

#if defined(X86)
#   include "x86/system.h"
#endif

/* system includes */
#ifdef _USE_SETJMP_
#include <setjmp.h>
#endif

/* if the stack for this processor decrements for a push , then set 
    TASK_STACK_GROWS to be TASK_STACK_PUSH_DOWN.  If the stack pointer 
    increments on push, the set it to TASK_STACK_PUSH_UP */
#define TASK_STACK_PUSH_UP      1
#define TASK_STACK_PUSH_DOWN    2
#define TASK_STACK_GROWS        TASK_STACK_PUSH_DOWN

/* Default stack size in bytes.  Minimum required for system is about 2.5K
    bytes. Original setting is 3K. */
#define DEFAULT_STACK_SIZE  3172
/* Default heap size in bytes.  The minimum is the DEFAULT_STACK_SIZE plus a
    few bytes.  Note that the stack is allocated from the heap.  No memory 
    is allocated by the task unless it sends or receives messages or uses 
    semaphores.  The default should add a K or two.  Original setting is 4K.*/
#define DEFAULT_HEAP_SIZE   4096
/* This is the priority that the system will set the user's task_main() to
    be. */
#define DEFAULT_TASK_PRIORITY   200

#ifdef _USE_SETJMP_
#define UINT_SIZEOF_CONTEXT (sizeof(jmp_buf)/sizeof(UINT))
#else
#define UINT_SIZEOF_CONTEXT 12
#endif

/******************************************************************************
*   Constants
*/

/* just in case the stdio header was not included... */
#ifndef NULL
#define NULL    ((void *)0)
#endif

/* Flag handler definitions */
#define SETFLAG(v, f)       ((v)=(v)|(f))
#define CLEARFLAG(v, f)     ((v)=(v)&(~(f)))
#define TOGGLEFLAG(v, f)    ((v)=(v)^(f))
#define TESTFLAG(v, f)      ((v)&(f))

#define HEAP_MAGIC          0xABADFADE
#define HEAP_STATUS_FREE    0x01
#define HEAP_STATUS_USED    0x02
#define HEAP_MIN_SIZE       1024
#define HEAP_MIN_NODE_SIZE  24
#define HEAP_PTR_TO_HCB(ptr)    ((HCB *)(((UINT)(ptr))-sizeof(HCB)))
#define HEAP_HCB_TO_PTR(hcb)    ((void *)(((UINT)(hcb))+sizeof(HCB)))

/*  make sure the status can never be wrapped around past the maximum for the
    data type  */
#define INCR_STATUS(tcb)    (((tcb)->status + 1) > (tcb)->status)? \
                            (tcb)->status + 1: (tcb)->status

/* make sure that the status can never be wrapped around past zero */                            
#define DECR_STATUS(tcb)    (((tcb)->status - 1) < (tcb)->status)? \
                            (tcb)->status - 1: (tcb)->status

/* section for signal.c */
#define MAX_SIGNALS     16
#define SIGNAL_KILL     0
#define SIGNAL_RUN      1
#define SIGNAL_BLOCK    2

/* task status consants */
#define TASK_RUNABLE        0
#define TASK_SUSPENDED      1
#define TASK_KILLED         ((UCHAR)-1)

/* bit flags */
#define WAIT_FOR_EVENT      0x01

/*  Events */
#define INVALID_EVENT           0x1000
#define GENERIC_EVENT           0x1001
#define MESSAGE_ARRIVAL_EVENT   0x1002
#define MESSAGE_RECIPT_EVENT    0x1003
#define SEMAPHORE_EVENT         0x1004
#define SIGNAL_EVENT            0x1005

/*  Return Codes */
#define TASK_ERROR          0xFFFFFFFF
#define TASK_SUCCESS        0x00000000

/*  Miscalenous */
#define TASK_DEFAULT_TCB    NULL
#define TASK_STACK_MAGIC    0x5A

/* message types */
#define NORMAL_MESSAGE      0x01

/* message flag */
#define MESSAGE_RECIPT_REQUEST  0x01
#define MESSAGE_NO_RECIPT       0x00

/******************************************************************************
*   Types
*/

/* generic types */
typedef unsigned int    UINT;
typedef unsigned char   UCHAR;
typedef UINT (*TASK_ENTRY)(void *);
typedef void (*SIG_FUNC)(void);
#ifdef _USE_SETJMP_
typedef jmp_buf TASK_CONTEXT;
#else
typedef UINT TASK_CONTEXT[UINT_SIZEOF_CONTEXT];
#endif

/* basic heap allocation data structure */
typedef struct __hcb__ {
    UINT magic;     /* used for error checking */
    UCHAR status;   /* allocated/free flag */
    UINT start;     /* index of the start of this data structure in the heap
                        buffer */
    UINT size;      /* size of this allocated or free chunk, incl the header */
    UCHAR data[1];  /* first byte of data that is accessable to the user */
} HCB;

/* basic heap data structure */
typedef struct __heap__ {
    UINT size;      /* total size of the heap */
    UINT address;   /* statrting address of this data structure */
    UCHAR data[1];  /* start of the memory to be managed */
} HEAP;

typedef struct __mq__ {
    UINT num_msgs;
    struct __msg__ *first, *last;
} MESSAGE_QUEUE;

typedef struct __eq__ {
    UINT num_events;
    struct __ev__ *first, *last;
} EVENT_QUEUE;

/* types */
typedef struct __tcb__ {
    /* task housekeeping */
    UINT task_number; 
    UINT *stack;
    UINT ssize;
    HEAP *heap;
    UINT hsize;
    UCHAR priority;
    int status;    /* could be a (-) number */
    UCHAR flags;
    
    /* context for tasks */
    TASK_CONTEXT context;
    TASK_ENTRY entry;
    void *arg;
    
    /* event queue where events are placed by the event_task() */
    EVENT_QUEUE *event_queue;
        
    /* pointers for scheduler lists */
    struct __tcb__ *tnext, *tprev;
    
    /* pad it out to an even word boundry */
} __attribute__ ((aligned(32), packed)) TCB;

typedef struct __tq__ {
    TCB *first, *last;
} TASK_QUEUE;

typedef struct __ev__ {
    UINT type;
    UINT subtype;
    TCB *sender;
    TCB *destination;
    struct __ev__ *next;
} __attribute__ ((aligned(32), packed)) EVENT;

/* section for message.c */
typedef struct __msg__ {
    UCHAR msg_type; /* so the receiver can tell what the sender meant. */
    UCHAR msg_flag; /* recipt request flag */
    TCB *sender;    /* TCB of the sender of the message */
    TCB *destination;   /* TCB of the intended recipiant */
    void *msg;      /* actual data in the message */
    UINT size;      /* size of the data */
    HEAP *heap;     /* heap that the nessage is currently allocated for */
    struct __msg__ *next;   /* pointer to the next message */
} __attribute__ ((aligned(32), packed)) MESSAGE;

typedef struct __cl__ {
    int argc;
    char **argv;
} CMDLINE;

/******************************************************************************
*   Extern declarations for global.c
extern UCHAR current_priority;
extern HEAP *global_heap;
*/


/******************************************************************************
*   Function protos
*/
#include "protos.h"

#endif  /* __KERN_HEADER_DEFINED__ */
