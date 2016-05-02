/*****************************************************************************\
*
*   These functions are used to control tasks.  In order to run the tasker,
*   the user must create a funciton called task_main().  The normal main()
*   function is defined here.  If it is defined in the user's program, there
*   will be a duplicate symbol error given by the linker.  See below for
*   more informaiton.
*
*   Note that the tasker it's self is does not need to be reentrant, but most 
*   other code does.
*
\*****************************************************************************/
/* set this to 1 to remove all of the normal C library calls such as
    printf() */
#define __RUN_AS_KERNEL__ 0

/* this will be set to 1 if you want to quit if no tasks are runable.
    otherwise, set it to zero.  (embedded = 0) 
#define __QUIT_NO_RUNABLE__ 1
*/

#if ! __RUN_AS_KERNEL__
#include <stdio.h>
#endif

#include "kern.h"

/*  This macro is for this module only.  It gets a pointer to the next task.  
    If the next pointer is NULL, then it returns a pointer to the first 
    task. */
#define NEXT_TASK(t)    (((t)->tnext == NULL)? task_queue.first: t->tnext)

/* module constants and local data */
static TASK_QUEUE task_queue = {NULL, NULL};
static TCB *current_task;
static jmp_buf sched_context;
static UINT task_number = 0;
static UCHAR task_crit_flag = 0;

/*  Functions that are used only by this module */
static void task_queue_add(TCB *tcb);
static int task_queue_del(TCB *tcb);
static void task_entry_address(void);
static void scheduler(void);
static void system_yield(int code);
static inline UCHAR get_sched_priority(void);
static inline TCB *delete_task(TCB *tcb);

/* defined in user application */
extern void task_main(CMDLINE *cl);

/*  private function defined in another module */
extern int init_global_heap(UCHAR *start, UINT size);
extern int init_event_system(void);

/******************************************************************************
*
*   This is the main funciton of the program.  The user needs to create a
*   function named task_main().  Task main is a normal task that is created
*   at priority 250.  It is always the first task that is created, so it is
*   guarenteed to run at least once.  It receives a argument that contains
*   the command line parameters in the CMDLINE data structure.  After it
*   creates all of the other tasks. then task_main() can either return or
*   continue in a loop, performing some other function.
*
*   Prototype of the user's main function:
*   void task_main(CMDLINE *cline);
*
*   In addition to creating the user's main task, this funciton also 
*   initializes the entire tasker system.  Other initialization needed by
*   the user's application should be done in the task_main() funciton.
*
*/
int main(int argc, char **argv) {

    static CMDLINE args;    /* persistant pointer */
    TCB *tcb;

#if !__RUN_AS_KERNEL__
    /* Test code allocates 2 meg for system memory */
    UCHAR *SYSTEM_MEMORY;
    UINT SYSTEM_MEMORY_SIZE = 2 * 1024 * 1024;
    extern void *malloc(unsigned int size);
    if((SYSTEM_MEMORY = malloc(SYSTEM_MEMORY_SIZE)) == NULL) {
        return TASK_ERROR;
    }
    printf("main = 0x%08X\n", (UINT)main);
#endif

    /* Init the global heap from system constants.  This _MUST_ be called
        before any of the other tasker funcitons that use memory. */
    if(init_global_heap(SYSTEM_MEMORY, SYSTEM_MEMORY_SIZE) != TASK_SUCCESS) {
        return TASK_ERROR;  /* this suould never really fail.... */
    }

    /*  Create this before other functions to make sure that the events will 
        be handled.  Otherwise, the tasker could jump off into the weeds.  */
    if(init_event_system() != TASK_SUCCESS) {
        return TASK_ERROR;
    }

    /* set up the user's main task */
    args.argc = argc;
    args.argv = argv;
    if((tcb = task_create(task_main,
                         (void *)&args,
                         DEFAULT_STACK_SIZE,
                         DEFAULT_HEAP_SIZE,
                         250)) == NULL) {
        return TASK_ERROR;
    }

    /*  Call the scheduler.  This function should never return except in the
        case of an error or if the kernel is running under another OS. */
    scheduler();
    /*printf("Tasker exiting\n");    */

    /*  If running as an embedded system, this should absolutely NEVER happen. 
        All errors that could cause the scheduler to return should be handled 
        so that it doesn't because there is nothin to return to. */
    return 0;                    
}


/******************************************************************************
*
*   Create a new task and put it in the task queue.
*
*   Note that this funciton does not yield to the scheduler as other system
*   calls do.  This is to allow for multiple tasks to be created by lower 
*   priority tasks.  Typically initialization tasks are low priority, and so 
*   the tasks that are created by it would need to run to completion before 
*   any other tasks could be created.
*
*   If one wishes to override the default functionality, and force a a task
*   switch after creating a task, then call yield() explicitly.
*
*   This function allocates several memory objects from the global heap.  
*   When a task returns or is killed, all of this memory is free'd by the 
*   system.
*   
*   This funciton uses the C goto keyword.  After taking a close look, it 
*   seems to me that this is a good (perhaps the only good) use of goto.  
*   It is used to handle the only error condition that is fatal to this 
*   function.  That is, a failure to allocate memory.  Since this is intended 
*   to be an embedded application, one should be aware in advance of how much 
*   memory is available and how much is required.  Therefore, this should 
*   never happen is real life except in the case of hardware failure.  Then 
*   we would probably not be executing this code any way.  In this case the 
*   use of a goto makes the code much more readable by getting rid of clutter.
*/
TCB *task_create(void *entry,
                 void *arg,
                 UINT stksize,
                 UINT hsize,
                 UCHAR prio) {

    TCB *tcb = NULL;
    void *heap_tmp = NULL;
    
    /* create the TCB from the global heap */
    if((tcb = global_alloc(sizeof(TCB))) == NULL) 
        goto handle_error;

    /*  Allocate and init the task's heap */
    if((heap_tmp = global_alloc(hsize)) == NULL) 
        goto handle_error;
    /*  we now have a pointer and a size, so init the task's heap */
    if((tcb->heap = init_heap(heap_tmp, hsize)) == NULL) 
        goto handle_error;

    /*  Set up the stack from the task's heap*/
    if((tcb->stack = tcb_alloc(tcb, stksize)) == NULL) 
        goto handle_error;

    /*  Note that events are always allocated on the global heap */
    if((tcb->event_queue = tcb_alloc(tcb, sizeof(EVENT_QUEUE))) == NULL)
        goto handle_error;

    /*  Do some initshit */
    tcb->ssize = stksize;

    /*  System dependant code! */
    if(setup_stack_frame(tcb, (void *)task_entry_address)) 
        goto handle_error;

    /*  Set some pointers */
    tcb->entry = entry;
    tcb->arg = arg;
    tcb->tnext = NULL;
    tcb->tprev = NULL;

    /*  Set the status.  New tasks created runnable. */
    tcb->status = TASK_RUNABLE;
    tcb->flags = 0;

    /*  Set up the task prioirty */
    tcb->priority = prio;
    /*  Give the task a serial number */
    tcb->task_number = task_number++;

    /*  Put it in the list */
    task_queue_add(tcb);

    /*  Return the handle */
    return tcb;

/*  Local GOTO destination for error handling */
handle_error:
    /* If the tcb was allocate successfully, then free it. */
    if(tcb != NULL)         global_free(tcb);
    /* If the heap was allocate successfully, then free it. */
    if(heap_tmp != NULL)    global_free(heap_tmp);
    /*  there is no need to free the stack upon error because it gets free()d 
        as a block inside the heap.  No further house keeping is required. */        
    return NULL;
        
}

/******************************************************************************
*
*   Mark the task as having been killed. It will be destroyed by the 
*   scheduler next time it runs.
*/
int task_kill(TCB *tcb) {

    /*  If the parameter is NULL, then it kill the caller. */
    if(tcb == NULL) {
        if((tcb = get_current_task_tcb()) == NULL)
            return TASK_ERROR;
    }

    /*  Update the status.  */
    tcb->status = TASK_KILLED;
    /*  Enter the scheduler like a good system call. */
    system_yield(1);

    /*  This function never returns except in the case of an error.  This is 
        here to avoid warnings when the -Wall parameter is used. */
    return TASK_SUCCESS;
}


/******************************************************************************
*
*   Get a task's priority.
*/
int task_get_priority(TCB *tcb) {

    /*  Pass a NULL to this function for a task to discover it's own 
        priority.  */
    if(tcb == NULL) {
        if((tcb = get_current_task_tcb()) == NULL)
            return TASK_ERROR;
    }
    /*  Enter the scheduler like a good system call. */
    system_yield(1);

    /*  This function returns only after the caller task gets re-scheduled */
    return (int)tcb->priority;
}


/******************************************************************************
*
*   Set a task's priority.
*/
void task_set_priority(TCB *tcb, UCHAR prio) {

    /*  Pass a NULL to this function for a task to set it's own 
        priority.  */
    if(tcb == NULL) {
        if((tcb = get_current_task_tcb()) == NULL)
            return;
    }
    
    /*  Enter the scheduler like a good system call. */
    tcb->priority = prio;
    
    /*  Enter the scheduler like a good system call. */
    system_yield(1);
}


/******************************************************************************
*
*   Return the task's current status.  Mostly used by scheduler.
*/
int task_get_status(TCB *tcb) {

    /*  Pass a NULL to this function for a task to discover it's own 
        status.  Actually, this will always return 0, because a task can 
        only get it's own priority if it is running.  */
    if(tcb == NULL) {
        if((tcb = get_current_task_tcb()) == NULL)
            return TASK_ERROR;
    }
    /*  Enter the scheduler like a good system call. */
    system_yield(1);

    /*  This function returns only after the caller task gets re-scheduled */
    return (int)tcb->status;
}

/******************************************************************************
*
*   Set the handle of the specified task.
*/
int task_set_status(TCB *tcb, int status) {

    /*  Pass a NULL to this function for a task to set it's own 
        status.  */
    if(tcb == NULL) {
        if((tcb = get_current_task_tcb()) == NULL)
            return TASK_ERROR;
    }

    tcb->status = status;
    /*  Enter the scheduler like a good system call. */
    system_yield(1);

    /*  This function returns only after the caller task gets re-scheduled */
    return TASK_SUCCESS;
}


/******************************************************************************
*
*   Return the TCB of the currently running task.
*/
TCB *get_current_task_tcb(void) {
    /*  Do not yield to the system because this function is used in other 
        tasker system calls that do yield.  */
    return current_task;
}

/******************************************************************************
*
*   Call the scheduler.  This is the only way for a user application to call 
*   it.  All other ways are to be considered dangerous.  If a user task wants 
*   to indicate a fatal system error, then it should return something other 
*   than TASK_SUCCESS.
*   
*/
void yield(void) {

    /*  Just hide the gory details. */
    system_yield(1);
}


/******************************************************************************
*
*   Free the memory associated with a task.  This will be more complicated
*   in the future.
*/
void free_task_resources(TCB *tcb) {

    /*  Free the TCB's heap.  No need to free the stack and such because the
        whole task heap is being destroyed. */
    global_free(tcb->heap);
    /* Free the TCB from the global heap */
    global_free(tcb);
}


/******************************************************************************
*
*   Start the task critical area.  This function has the effect of preventing
*   system calls from causing a task switch.  It should be mostly useless
*   because good software design makes it unnessesary.  Be sure to call
*   "task_end_critical()" as soon as possable after issuing this call.
*/
void task_start_critical(void) {

    task_crit_flag = 1;
}


/******************************************************************************
*
*   End the task critical area.  Task switches are allowed after this call.
*/
void task_end_critical(void) {

    task_crit_flag = 0;
}


/******************************************************************************
*   STATIC FUNCITONS
*/
/******************************************************************************
*
*   Internal yield.  So system calls can use a return code.
*
*/
static void system_yield(int code) {

    /*  If the task has asked not to change context, honor the request */
    if(task_crit_flag == 0) {
        /* if this was a call and not a non-local GOTO */
        if(save_task_context(current_task->context) == 0) {        
            restore_task_context(sched_context, code);
        }
    }
}

/******************************************************************************
*
*   Scheduler.  This is the heart of the tasking kernel.  It is where the
*   decision of what task is runnable is made.  
*
*   This is a priority based scheduler.  That is, the task that has the highest
*   priority, and is not blocked, is given the first opportunity to run.  Tasks 
*   with the same priority, that are not blocked, are made to run in round-
*   robbin fasion.  The scheduler could be said to be "running at a specific
*   priority". 
*
*   Tasks are kept in a queue or list.  The task queue holds all of the tasks 
*   that have been created and have not been destroyed.  
*
*   The scheduler uses the following steps to schedule a task.
*   1.  The current highest priority is determined with a call to the funciton
*       "get_sched_priority()".  This funciton also deletes any tasks that have
*       been killed or that have returned since the last time the scheduler was
*       called.
*
*   2.  The next task after the current_task that is not blocked and that has
*       the priority that was indicated by "get_sched_priority()" is selected 
*       to run.
*
*   3.  The current_task global is a pointer to the currently running task.  It
*       is used to indicate to other parts of the system which task is currently
*       running as well as keeping track where we are in the task queue so that
*       the entire queue does not need to be examined to find a runnable task.
*
*   4.  If all of the tasks in the task queue are blocked, then a system
*       specific action is taken.  
*           a.  If the tasker is running as an application under another 
*               operating system, then the program should be put to sleep or 
*               it should return to the system.  
*           b.  If the tasker is running as a stand-alone system, then it should
*               halt the processor, pending an interrupt.  If this is not
*               possable, then it should loop until a task is made runnable by
*               some external event.
*       In any case, the scheduler should not schedule another task until some 
*       external event takes place that makes a task runnable.
*
*   5.  If there are no tasks in the task queue, either because they have all
*       been killed or they have returned, or because the scheduler was called
*       before the main task has been created (a programmer error), then a 
*       fatal error should be envloked, perhaps returning to some kind of 
*       debug monitor or returning to the host OS if applicable.
*
*   General strucutre of the scheduler:
*
*   What happens in this funciton is a little complicated, but not extremely 
*   so.  The next runnable task is selected, the context of the scheduler is
*   saved so that "yield()" can cause a non-local GOTO to it.  Then the context
*   of the lucky task is restored, causing a non-local jump to it.  When 
*   "yield()" is called, it looks to the scheduler as though 
*   "save_task_context()" was just called and that it returned a non-zero 
*   value.  If "save_task_context()" was called by the scheduler, then it is
*   required to return zero.  "save_task_context()" and "restore_task_context()"
*   behave just as "setjmp()" and "longjmp()" do.  In fact, for some situations,
*   the context functions are a macro that causes setjmp and longjmp to be used.
*   This is one of the ways that this OS achives good portability.
*
*/
static void scheduler(void) {

    UCHAR current_priority;
    TCB *tcb;
    UINT retv;
    
    while(1) {    
        /*  If this is true, then there are no tasks that are runnable. */
        if((current_priority = get_sched_priority()) == TASK_KILLED) {
        
/*  TODO: Fix this so that the __QUIT_NO_RUNABLE__ variable works.  See the
    comment above where it talks about system dependant responces to not having
    a runnable task.  */
    
            return;
        }
        
        /*  When we reach here, it is certain that there is a runable task, 
            so find it. */
        tcb = NEXT_TASK(current_task);
        while(tcb->priority > current_priority || tcb->status != 0) {
            tcb = NEXT_TASK(tcb);
        }
        
        /*  At this point "tcb" should point to a runable task, so involk it.   
            The implication is that tcb->priority <= current_priority and
            that the task is not blocked. (status == 0) */
        
        /*  If this is true, then a task has returned a fatal error, return to
            the "main" procedure. Note that when a task does a "yield()" here
            is where control enters with a value that is not zero and (hopeflly)
            not TASK_ERROR. The shcheduler context is saved no matter what 
            "retv" turns out to be.  */
        if((retv = save_task_context(sched_context)) == TASK_ERROR)
            return;
        /*  Else, if "retv" is zero, then "save_task_conext()" was not a
            non-local GOTO.  Do a non-local GOTO to the runable task. */
        else if(retv == 0) {
            current_task = tcb;
            restore_task_context(tcb->context, 1);
        }
        /*  else ignore other values */
        
/*  TODO:  Provide error handlers that handle error codes from tasks that wish
    to return them.  */        
        
    }    

    /* No return from this function is likely.  */
}

/******************************************************************************
*
*   Stupidly find out what the highest runnable priority is and return it. 
*   Since this function looks at every single task, then it is a logical place
*   to handle killed tasks as well.
*
*   Note that this is not a permanent way to do this.  It is here just to get
*   things running until I can think of a better way to do it.  The problem is
*   that this function touches every single task in the task queue.  It should 
*   be possable to do it incrementally or by sorting the task list.  Another
*   way may include defining another data structure so that a list of 
*   priorities can be scanned.  
*
*   i.e.
*   struct {
*       int priority;
*       char flag;
*       TCB *first, *last;
*   } PLIST;
*
*   The priority is the prirority of the list of tasks, which all have the same
*   priority.  The flag is set when there is a task at this priority that is
*   runnable.  The list of priorites is potentially much shorter than the list 
*   of tasks.
*
*/
static inline UCHAR get_sched_priority(void) {

    TCB *tcb;
    UCHAR priority = 0xFF;
    
    for(tcb = task_queue.first; tcb != NULL; tcb = tcb->tnext) {
    
        /*  Delete all of the killed tasks */
        if(tcb->status == TASK_KILLED) {        
            tcb = delete_task(tcb);
            if(tcb == NULL)
                return TASK_KILLED;
        }

        /*  Update the priority.  Find the lowest value. (highest priority) */ 
        if(tcb->status == 0 && (priority > tcb->priority))
            priority = tcb->priority;
    }
    
    /*  Return the priority that the scheduler should schedule.  */
    return priority;
}


/******************************************************************************
*
*   Delete the TCB in question from the task list.  Returns a pointer to the
*   next task to avoid indirection on deleted data.
*/
static inline TCB *delete_task(TCB *tcb) {

    TCB *tmp_tcb;
    
    /*  get the next one before we delete this one. avoids 
        indirection on a free'd pointer. */
    if((tmp_tcb = NEXT_TASK(tcb)) == NULL) {
        return NULL;
    }
            
    /* delete the TCB from the list */            
    task_queue_del(tcb);
                
    /* free it's memory */
    free_task_resources(tcb);
            
    return tmp_tcb;
}


/******************************************************************************
*
*   This is the entry address for starting tasks.  
*/
static void task_entry_address(void) {

    TCB *tcb;
    UINT retv;

    /*  The task will have been scheduled, so find out the TCB address */    
    tcb = get_current_task_tcb();
    
    retv = (*tcb->entry)(tcb->arg); /* jump to the task */

    tcb->status = TASK_KILLED;      /* set the killed status if it returns */

    system_yield(retv);             /* return to the scheduler normally */
}


/******************************************************************************
*
*   Add a task to the task queue.  Items are always added to the end of the
*   list.  The scheduler places no value on the location of an item in this
*   list.  Adding it to the end is easy.
*/
static void task_queue_add(TCB *tcb) {

    if(task_queue.last == NULL) {
        task_queue.last = tcb;
        task_queue.first = tcb;
        current_task = tcb;
        tcb->tnext = NULL;
        tcb->tprev = NULL;
    }
    else {
        task_queue.last->tnext = tcb;
        tcb->tprev = task_queue.last;
        tcb->tnext = NULL;
        task_queue.last = tcb;
    }
}

/******************************************************************************
*
*   Delete the specified item from the task queue.  Do not destroy the item,
*   only remove it from the list.  This function is why the queue uses a 
*   double linked list.  If we are deleting from the middle of the list, it 
*   is easier to correctly update the list if it is double linked.
*/
static int task_queue_del(TCB *tcb) {

    if(tcb->task_number == current_task->task_number) {
        current_task = NEXT_TASK(current_task);
    }
    
    if(tcb->task_number == task_queue.first->task_number) {
        task_queue.first = task_queue.first->tnext;
        if(task_queue.first == NULL)
            task_queue.last = NULL;
        else
            task_queue.first->tprev = NULL;
    }
    else if(tcb == task_queue.last) {
        task_queue.last = task_queue.last->tprev;
        if(task_queue.last == NULL)
            task_queue.first = NULL;
        else
            task_queue.last->tnext = NULL;
    }
    else {
        if(tcb->tnext != NULL) {
            tcb->tnext->tprev = tcb->tprev;
        }

        if(tcb->tprev != NULL) {
            tcb->tprev->tnext = tcb->tnext;
        }
    }

    tcb->tprev = NULL;
    tcb->tnext = NULL;

    if(task_queue.first == NULL)
        return 1;
    else
        return 0;
}


#ifndef __RUN_AS_KERNEL__
/******************************************************************************
*   Function used to test the list.  No other use that I know of..... 
*/
int show_task_queue(void) {

    TCB *t;
    int i;

    for(i = 0, t = task_queue.first; t != NULL; i++, t = t->tnext) {
        printf("tcb = %p\n  tcb->tprev = %p\n  tcb->tnext = %p\n",
               t, t->tprev, t->tnext);
    }
    printf("%d tasks in queue\n", i);

    return TASK_SUCCESS;
}
#endif


/* end of task.c */


