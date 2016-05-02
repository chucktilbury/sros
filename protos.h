/******************************************************************************
*
*   Interface function prototypes and associated documentation.
*
*/
#ifndef __PROTO_HEADER_DEFINED__
#define __PROTO_HEADER_DEFINED__


/* defined in util.c */
/******************************************************************************
*
*   Copy memory efficiently from the src pointer to the dest pointer.  This
*   function trys to copy using the default register size of the processor,
*   using bytes to copy the rest if required.
*
*   Parameters:
*       void *dest      Pointer to the buffer to use as the source of the data
*                       to copy.
*
*       void *src       Pointer to where to put the data.
*
*       UINT bytes      Number of bytes to copy.
*
*   Returns:
*       nothing
*
*   Example:
*       copy_memory(destination, source, 1024);
*
*/
void copy_memory(void *dest, void *src, UINT bytes);

/* defined in task.c */
/******************************************************************************
*
*   Create a new task.  The task is created as runable and placed in the queue.
*   Next time the scheduler is called, the task will run if it has the highest
*   priority of all runable tasks.  This system call does not cause a task
*   switch as most of the others do.  This is to allow a low priority task to
*   create other higher priority tasks.
*
*   Parameters:
*       void *entry     The entry point into the function that makes up the
*                       task.  The function that this points to should never
*                       be explicitly called.  It should only be entered by the
*                       scheduler.
*
*       void *arg       Tasks can accept a single pointer as an argument.  It 
*                       can be any data type that has meaning to the task.
*
*       UINT stksize    The stack size to allocate from the task's heap.  If
*                       this value is too small, then unpredictable behavior
*                       results.  Tasks should use a predictable stack size.
*                       Recursion in a function that the task calls should be 
*                       avoided.  
*
*       UINT hsize      Size of the heap that this task will use.  It should 
*                       large enough to handle the stack that the task will use
*                       as well as the other memory that this task will allocate
*                       using "task_alloc()" or "tcb_alloc()".
*
*       UCHAR prio      Priority that the task will be created with.  The
*                       highest priroity one may reasonably use would be 10. 
*                       The lowest priority would be 250.  The user's main task
*                       "task_main()" is created with a priority of 250.
*
*   Returns:
*       If there was no error, then return a pointer to the new Task Control
*       Block (TCB).  If there was an error, such as not having enough memory, 
*       then NULL is returned, indicating that there was no task created.
*
*   Example:
*       if((tcb = task_create(task_main,
*                         (void *)&args,
*                         DEFAULT_STACK_SIZE,
*                         DEFAULT_HEAP_SIZE,
*                         250)) == NULL) {
*           return TASK_ERROR;
*       }
*
*/
TCB *task_create(void *entry,
                 void *arg,
                 UINT stksize,
                 UINT hsize,
                 UCHAR prio);

/******************************************************************************
*
*   Causes the specified task to be marked as killed.  This system call also 
*   could cause a task switch.  The task is not deleted until the scheduler is
*   called again.  Deleteing a task is permanent.  It cannot be "undeleted" or
*   recovered in any way.
*
*   This function causes a task switch when it is called.
*
*   Parameters:
*       TCB *tcb        Pointer to the task control block of the task to kill.
*                       If this parameter is NULL, then the task that called 
*                       this function kills it's self.
*
*   Returns:
*       If there was no error, then return TASK_SUCCESS.  Else return
*       TASK_ERROR.
*
*   Example:
*       task_kill(NULL);
*
*/
int task_kill(TCB *tcb);

/******************************************************************************
*
*   Return the current status of the specified task.  Any task can find the
*   status of any other task in the system as long as it knows the TCB pointer 
*   of the task to query.
*
*   This function causes a task switch when it is called.
*
*   Parameters:
*       TCB *tcb        Pointer to the task control block of the task to 
*                       query.  If this parameter is NULL, then the task 
*                       that called this function gets it's own status, which
*                       always be 0, since the task is running.
*
*   Returns:
*       If there was no error, then return the status of the task in question.
*       Else return TASK_ERROR.  (a task's status can never equal TASK_ERROR)
*
*   Example:
*       status = task_get_status(tcb);
*
*/
int task_get_status(TCB *tcb);

/******************************************************************************
*
*   This function is the  compliment of the "task_get_status()" function.  You
*   can use to specifically set the status of a task.  Note that if the task 
*   that this function is called against has previously been blocked by an 
*   event, then is possable that some information about the event will be lost,
*   possably leading to a system crash.  Therefore, this function should only 
*   be used against tasks to do not use events.  The task that was blocked can 
*   be un-blocked with a call to this same function. (see "status" below)
*
*   This function causes a task switch when it is called.
*
*   Parameters:
*       TCB *tcb        Pointer to the task control block of the task to 
*                       control.  If this parameter is NULL, then the task 
*                       that called this function sets it's own status.
*
*       int status      The status to set for the specified task.  The only 
*                       values that actually make sense are "TASK_RUNABLE" 
*                       and "TASK_SUSPENDED".  See kern.h for the definitions 
*                       and possably more information.
*
*   Returns:
*       If there was no error, then return TASK_SUCCESS.  Else return
*       TASK_ERROR.
*
*   Example:
*       result = task_set_status(tcb, TASK_SUSPENDED);
*
*/
int task_set_status(TCB *tcb, int status);

/******************************************************************************
*
*   Return the priority of the specified task.
*
*   Parameters:
*       TCB *tcb        Pointer to the task control block of the task to 
*                       query. If this parameter is NULL, then the task that 
*                       called this function get it's own priority.
*
*   Returns:
*       If there was no error, then return the priority of the task in question.
*       Else return TASK_ERROR.  (a task's priority can never equal TASK_ERROR)
*
*   Example:
*       priority = task_get_priority(tcb);
*
*/
int task_get_priority(TCB *tcb);

/******************************************************************************
*
*   Return the TCB pointer of the currrently running task.  This is
*   useful for a task to find out it's own TCB so it can do something to it.
*   This function is widely used in system calls, so it does not produce a task 
*   switch.  NOTE that a task manipulating it's own TCB with out the use of 
*   the system interface should never be considered good coding practice. 
*
*   Parameters:
*       None.
*
*   Returns:
*       Pointer to the current TCB, or else NULL if there is an error.  Note 
*       that if this function ever returns NULL, you have bigger problems than 
*       a lost TCB pointer....
*
*   Example:
*       tcb = get_current_task_tcb();
*
*/
TCB *get_current_task_tcb(void);

/******************************************************************************
*
*   Call the scheduler.  This is the only way for a user application to
*   explicitly give up the CPU.  Note that almost all system calls also give up
*   the CPU so other tasks can run.
*
*   Parameters:
*       none.
*
*   Returns:
*       nothing.
*
*   Example:
*       yield();
*
*/
void yield(void);

/******************************************************************************
*
*   Prevent system calls from calling the scheduler.  This funciton may be
*   required for some situations, but I expect that they will be rare.  In any
*   case, if this function is used, then it's compliment, "task_end_critical()"
*   should be called as soon after as possable.  If this function is called 
*   and "task_end_critical()" is never called, then task switching is turned 
*   off permanently.
*
*   Parameters:
*       none.
*
*   Returns:
*       nothing.
*
*   Example:
*       task_start_critical();
*
*/
void task_start_critical(void);

/******************************************************************************
*
*   Causes task switching to be re-enabled after a call to 
*   "task_start_critical()".
*
*   Parameters:
*       none.
*
*   Returns:
*       nothing.
*
*   Example:
*       task_end_critical();
*
*/
void task_end_critical(void);


/* defined in memory.c */
/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
HEAP *init_heap(UCHAR *start, UINT size);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
void *global_alloc(UINT size);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
void *global_realloc(void *ptr, UINT size);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
int global_free(void *ptr);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
void *task_alloc(UINT size);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
void *task_realloc(void *ptr, UINT size);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
int task_free(void *ptr);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
void *tcb_alloc(TCB *tcb, UINT size);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
void *tcb_realloc(TCB *tcb, void *ptr, UINT size);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
int tcb_free(TCB *tcb, void *ptr);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
int heap_verify_node(HEAP *h, void *node);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
int heap_walk(HEAP *h);

/* defined in system.c */
/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
int setup_stack_frame(TCB *tcb, void *entry);

#ifndef _USE_SETJMP_
/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
int save_task_context(TASK_CONTEXT tc);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
void restore_task_context(TASK_CONTEXT tc, UINT code);
#endif

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
void halt_processor(void);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
int sys_check_stack(TCB *tcb);

/* defined in event.c */
/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
int generate_event(TCB *tcb, UINT type, UINT subtype);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
TCB *check_event(UINT *type, UINT *subtype);

/******************************************************************************
*
*   Function prototypes and associated documentation.
*
*
*   Parameters:
*
*   Returns:
*
*   Notes:
*
*/
TCB *wait_event(UINT *type, UINT *subtype);

#endif  /* __PROTO_HEADER_DEFINED__ */
