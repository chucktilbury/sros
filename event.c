/******************************************************************************
*
*   Event driver.  This is a system-level driver that all of the other OS 
*   inter-process communication drivers depend on.  The event system is made
*   up of an event task that communicates with other tasks.  It controls 
*   whether a given task is runable or not.  Notions such as waiting on a
*   semaphore are really event driven.  I.E. when a semaphore is released, an 
*   event is generated that communicates with the task that is waiting on it.
*
*/
#include "kern.h"

static EVENT_QUEUE *system_event_queue;
static TCB *event_task_tcb;

static void free_event(EVENT *event);
static EVENT *allocate_event(void);
static EVENT *dequeue_event(EVENT_QUEUE *eq);
static void enqueue_event(EVENT_QUEUE *eq, EVENT *event);
static int event_task(void);

/******************************************************************************
*
*   Init the event system.
*
*/
int init_event_system(void) {

    /*  Create the event queue used by the event task  */
    if((system_event_queue = global_alloc(sizeof(EVENT_QUEUE))) == NULL) {
        return TASK_ERROR;
    }
       
    /*  Create the event task */
    event_task_tcb = task_create(event_task, 
                                   NULL, 
                                   DEFAULT_STACK_SIZE,
                                   DEFAULT_HEAP_SIZE,
                                   0);
    /*  Check to make sure that the task was actually created.  In practice,
        this should never fail... */
    if(event_task_tcb == NULL) {
        global_free(system_event_queue);
        return TASK_ERROR;
    }
        
    /*  If we reached here, return success */
    return TASK_SUCCESS;
    
}


/******************************************************************************
*
*   Generate an event.
*
*   This funciton indicates to the event task that there was an event in the 
*   system.  It unblocks the event task and calls the scheduler.  Generating 
*   an event never blocks the sender.
*/
int generate_event(TCB *tcb, UINT type, UINT subtype) {

    EVENT *event;
    TCB *local_tcb;
    
    /*  If the tcb parameter is NULL, then use the current TCB.  Has the effect 
        of sending an event to the running task. */
    if(tcb == NULL) 
        local_tcb = get_current_task_tcb();
    else
        local_tcb = tcb;
        
    /*  Get the memory for the event from the global heap */
    if((event = allocate_event()) == NULL) 
        return TASK_ERROR;
        
    event->type = type;
    event->subtype = subtype;
    event->destination = local_tcb;
    event->sender = get_current_task_tcb();
    event->next = NULL;
    
    /*  Save it in the queue.  */
    enqueue_event(event_task_tcb->event_queue, event);
    /*  Tell the event task to run.  */
    event_task_tcb->status = TASK_RUNABLE;
    
    /*  Since the event task is a very high priority, it is practaclly certain
        this it will be the next task to run.  Do not, however, depend on it. */
    yield();
        
    return TASK_SUCCESS;
}

/******************************************************************************
*
*   Asyncrounously receive events.
*
*   If there is no event waiting, then return NULL and set the type to zero.
*   If an error happened, then return NULL and set the type to a non-zero error
*   code.  If there was an event, then return a pointer to the sender and set
*   the type to be the type of the event and the subtype to the sub-type.
*
*   For example, if the event was a signal, then the type will be SIGNAL_EVENT,
*   the subtype will be the signal number and the return value will the a 
*   pointer to the task that raised the signal.
*
*   (Signals are in the TODO catagory).
*/
TCB *check_event(UINT *type, UINT *subtype) {

    TCB *tcb = NULL;
    EVENT *event;
    
    /*  Operate on the current task only.  */
    tcb = get_current_task_tcb();
    
    /*  If this is true, then there are no events waiting to be delivered. */
    if(tcb->event_queue->num_events == 0) {
        *type = 0;
        return NULL;
    }
    
    /*  If we make it here, then there is an event.  Get it from the queue and
        return it just like the wait_event() funciton. */
    event = dequeue_event(tcb->event_queue);
    tcb = event->sender;
    *type = event->type;
    *subtype = event->subtype;
    
    /*  No longer required as it is returned by value. */
    free_event(event);
    
    /*  As with all system calls, this function causes a potential task 
        switch. */
    yield();
        
    /*  Return the pointer to the sender. */
    return tcb;    
}

/******************************************************************************
*
*   Syncronously receive events.
*
*   This is just like the asyncronous version except that it waits for an event
*   to be delivered before it returns.  If this function returns NULL, then
*   there was an error.  The type parameter is set to the non-zero error 
*   number.
*
*/
TCB *wait_event(UINT *type, UINT *subtype) {

    TCB *tcb = NULL;
    EVENT *event;
    
    /*  Operate on the current task only. */
    tcb = get_current_task_tcb();
    
    /*  Wait for the event by yielding to the scheduler if there is no event
        ready for delivery. */
    while(tcb->event_queue->num_events == 0) {
        tcb->status = INCR_STATUS(tcb);
        SETFLAG(tcb->flags, WAIT_FOR_EVENT);
        yield();
    }
    
    /*  when we reach here, then the event_task marked this task as runnable
        again. */
    event = dequeue_event(tcb->event_queue);
    tcb = event->sender;
    *type = event->type;
    *subtype = event->subtype;
    
    /*  No longer required as it is returned by value. */
    free_event(event);
    
    /*  Return the pointer to the sender. */
    return tcb;    
}


/******************************************************************************
*
*   STATIC FUNCTIONS
*
*/
/******************************************************************************
*
*   Event task.  The generate_event() function actually sends events to this
*   task.  This task then unblocks the receiver if nessesary and places the 
*   event in the receiver's event queue.  Then it blocks it's self and yields
*   the processor to other tasks.
*
*   This funciton could be considered a template for other tasks.
*
*/
static int event_task(void) {

    EVENT *event;

    /* loop forever... */
    while(1) {

        /* get each event, one at a time */
        while((event = dequeue_event(event_task_tcb->event_queue)) != NULL) {
/* TODO: do some sanity checking.... */

            /*  If the destination task is blocked for an event, then make it
                runable. */
            if(TESTFLAG(event->destination->flags, WAIT_FOR_EVENT)) {            
                event->destination->status = DECR_STATUS(event->destination);
                CLEARFLAG(event->destination->flags, WAIT_FOR_EVENT);
            }

            /* send the event to the destination */
            enqueue_event(event->destination->event_queue, event);
        }
                
        /* stop the event task */
        event_task_tcb->status = TASK_SUSPENDED;
        /* return to the scheduler */
        yield();
    }
    
    /* should be impossable, but.... */
    return TASK_ERROR;
}


/******************************************************************************
*
*   Enqueue an event.
*
*   Events are always added of the end of the linked list.
*
*/
static void enqueue_event(EVENT_QUEUE *eq, EVENT *event) {

    if(eq->last == NULL) {
        /* init the last and the first pointer in the queue. */
        eq->last = event;         
        eq->first = event;
    }
    else {
        /* else add it to the end of the queue */
        eq->last->next = event;
        eq->last = event;
        event->next = NULL;
    }
    
    eq->num_events++;
}


/******************************************************************************
*
*   Dequeue an event.
*
*   Events are always taken from the start of the linked list.
*
*/
static EVENT *dequeue_event(EVENT_QUEUE *eq) {

    EVENT *event;
    
    if(eq->first == NULL) {
        /*  There is no event to return so return NULL */
        event = NULL;
    }
    else {
        /*  Else there is an event, so remove it from the list and get ready
            to return a pointer to it.  */
        event = eq->first;
        eq->first = eq->first->next;
        /* check if the queue is empty. */
        if(eq->first == NULL) 
            /* indicate that condition to the add function */
            eq->last = NULL;
        eq->num_events--;
    }
    
    return event;
}


/******************************************************************************
*
*   Allocate an event on the global heap.  Return a pointer to it or NULL if
*   there was an error.  Simply a convience function to hide details.
*
*/
static EVENT *allocate_event(void) {

    return (EVENT *)global_alloc(sizeof(EVENT));
}


/******************************************************************************
*
*   Free an event from the global heap.  Since events are never returned to
*   a user task, this funciton is static in scope.
*
*/
static void free_event(EVENT *event) {

    global_free(event);
}





