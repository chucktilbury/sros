/******************************************************************************
*
*   This module contains functions that are specific to the specified
*   processor.  The functions in this module should be duplicated 
*   for every supported processor.
*
*   For: Intel x86 processors.
*/
#include "system.h"
#include "../kern.h"

/******************************************************************************
*
*   This function is called by task_create() to set up the stack frame.
*
*   Look at /usr/include/bits/setjmp.h for more information.
*
*   Stack decrements for push. Stack is organized as an array of 32 bit 
*   words for Intel processors. 
*
*/
int setup_stack_frame(TCB *tcb, void *entry) {

    int i;
    
#if 0
    (UINT *)tcb->jbuf = tcb->buf;
    /* make sure it is aligned on a word boundry! */
    tcb->buf[JB_SP] = (UINT)((UCHAR *)tcb->stack+(tcb->ssize - 1)) & 0xFFFFFFF0; 
    tcb->buf[JB_PC] = (UINT)entry;  /* set program counter */
#endif

    /* Fill the stack with a magic value so that it can be checked for 
        size. */
    for(i = 0; i < tcb->ssize; i++) {
        ((UCHAR *)tcb->stack)[i] = TASK_STACK_MAGIC;
    }

    /* set some (hopefully) reasonable values into the jmp_buf */
    if(save_task_context(tcb->context) != 0) {
        /* If we make it here, there was a catastrophic error somewhere.
            This should absolutely NEVER happen. If it did, then this
            was the result of a non-local GOTO.  Very, very evil... */
        return 1;
    }
    

    ((UINT *)tcb->context)[JB_SP] = 
                (UINT)((UCHAR *)tcb->stack+(tcb->ssize - 1)) & 0xFFFFFFF0; 
    ((UINT *)tcb->context)[JB_PC] = (UINT)entry;
    
    return 0;
}    

#if 0
/******************************************************************************
*
*   Save the task's context.
*
*/
int save_task_context(TASK_CONTEXT tc) {

    register UINT _AX_ asm ("eax");
    register UINT _BX_ asm ("ebx");
    register UINT _CX_ asm ("ecx");
    register UINT _DX_ asm ("edx");
    register UINT _SI_ asm ("esi");
    register UINT _DI_ asm ("edi");
    register UINT _BP_ asm ("ebp");
    register UINT _SP_ asm ("esp");
    UINT ctx[12];
        
    asm ("cli");
    ctx[0] = _AX_;
    ctx[1] = _BX_;
    ctx[2] = _CX_;
    ctx[3] = _DX_;
    ctx[4] = _SI_;
    ctx[5] = _DI_;
    ctx[6] = _BP_;
    ctx[7] = _SP_;
    asm ("sti");
    
    return setjmp(tc);
}


/******************************************************************************
*
*   Restore the task's context.
*
*/
void restore_task_context(TASK_CONTEXT tc, UINT code) {

    register UINT _AX_ asm ("eax");
    register UINT _BX_ asm ("ebx");
    register UINT _CX_ asm ("ecx");
    register UINT _DX_ asm ("edx");
    register UINT _SI_ asm ("esi");
    register UINT _DI_ asm ("edi");
    register UINT _BP_ asm ("ebp");
    register UINT _SP_ asm ("esp");

    asm ("cli");
    asm ("sti");

    longjmp(tc, code);
}
#endif

/******************************************************************************
*
*   Halt the processor.
*
*/
void halt_processor() {

    /* do nothing for now... */
}

/******************************************************************************
*
*   Check the aproximate amount of stack in use.  Count from the end of the
*   stack that is the bottom to get the number of bytes that have not been 
*   used.  Subtract it from the total size and returne the result.  If the 
*   first byte checked has been changed, then return -1.
*/
int sys_check_stack(TCB *tcb) {

    int val, i;

    /* check if we mean the currently running task */
    if(tcb == NULL) {
        if((tcb = get_current_task_tcb()) == NULL)
            return -1;
    }

    /* check for the bottom of the stack not having been used.  There should
        always be a little more stack than is actually being used */
    if(((UCHAR *)tcb->stack)[0] != TASK_STACK_MAGIC)
        return -2; /* stack overrun! */
        
    /* count backward (int the stack) until used bytes are found */
    for(i = 0; ((UCHAR *)tcb->stack)[i] == TASK_STACK_MAGIC; i++) {
        if(i > tcb->ssize)
            return -3;  /* this should never happen.  Some major internal error
                            must have taken place. */
    }
        
    /* so calculate it */
    val = tcb->ssize - i;
        
    return val;
}

