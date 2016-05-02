/*
*   test the task stuff
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "../kern.h"

void task1(char *str);
void task2(char *str);
void task3(char *str);
int event_task(void);
void check_stack_info(int a, int b);
extern HEAP *global_heap;
TCB *event_tcb;

void task_main(CMDLINE *cl) {

    int i, finished = 0;
    TCB *tcb;

    printf("task_main()!\n");
    for(i = 0; i < cl->argc; i++) {
        printf("arg %d = %s\n", i, cl->argv[i]);
    }

    if((tcb = task_create(task1, "task1",
                DEFAULT_STACK_SIZE,
                DEFAULT_HEAP_SIZE,
                51)) == NULL) {
        printf("cannot allocate task 1\n");
    }        

    if((tcb = task_create(task2, "task2",
                DEFAULT_STACK_SIZE,
                DEFAULT_HEAP_SIZE,
                51)) == NULL) {
        printf("cannot allocate task 2\n");
    }

    if((tcb = task_create(task3, "task3",
                DEFAULT_STACK_SIZE,
                DEFAULT_HEAP_SIZE,
                51)) == NULL) {
        printf("cannot allocate task 3\n");
    }        

    if((event_tcb = task_create(event_task, NULL,
                DEFAULT_STACK_SIZE,
                DEFAULT_HEAP_SIZE,
                50)) == NULL) {
        printf("cannot allocate task 4\n");
    }

    i = 0;
    while(!finished) {
        printf("mtask: stk = %d\n", sys_check_stack(NULL));
        yield();
        i++;
        if(i > 10)
            finished++;
    }
    
    generate_event(event_tcb, -1, -1);
}

void task1(char *str) {

    int i = 0;
    TCB *tcb;

    tcb = get_current_task_tcb();
    while(1) {
        printf("T1:%p:%d:%s:%d:%d\n", 
            tcb, 
            i++, 
            str, 
            task_get_priority(tcb),
            sys_check_stack(tcb));
    
        generate_event(event_tcb, 1, 100);
        if(i >= 2)
            break;
        //show_run_queue();
        yield();
    }
    printf("task 1 returning\n");

}

void task2(char *str) {

    int i = 0;
    TCB *tcb;

    tcb = get_current_task_tcb();
    while(1) {
        printf("T2:%p:%d:%s:%d:%d\n", 
            tcb, 
            i++, 
            str, 
            task_get_priority(tcb),
            sys_check_stack(tcb));

        generate_event(event_tcb, 2, 200);
        if(i >= 3)
            break;
        //show_run_queue();
        yield();
    }
    printf("task 2 returning\n");
}

void task3(char *str) {

    int i = 0;
    TCB *tcb;

    tcb = get_current_task_tcb();
    while(1) {
        printf("T3:%p:%d:%s:%d:%d\n", 
            tcb, 
            i++, 
            str, 
            task_get_priority(tcb),
            sys_check_stack(tcb));
            
        //raise_signal(NULL, SIGNAL_KILL);
        
        generate_event(event_tcb, 3, 300);
        if(i >= 7)
            break;
        //show_run_queue();
        yield();
    }
    printf("task 3 returning\n");
}

int event_task(void) {

    int type, stype;
    TCB *tcb, *etcb;

    printf("event receiver started\n");
    tcb = get_current_task_tcb();
    while(1) {
        etcb = wait_event(&type, &stype);
        printf("Event from 0x%08X: %d:%d\n", (UINT)etcb, type, stype);
        if(type == -1)
            break;
    }
    printf("user event task returning\n");
    
    return 0;
}

/* take a look at how the stack behaves for this processor */
void check_stack_info(int a, int b) {

    int check_stack_direction(UINT v);
    UINT v;
    char *fmt = "The %s function parameter pushed on to the stack first.\n";
    
    v = (UINT)&a > (UINT)&b;
    if(check_stack_direction((UINT)&v)) {
        printf(fmt, (v)? "last": "first");
    }
    else {
        printf(fmt, (v)? "first": "last");
    }
}

int check_stack_direction(UINT v) {

    UINT a;
    
    a = (UINT)&a > v;
    
    printf("Push instruction %s the stack pointer.\n", (a)?
              "increments": "decrements");  
              
    return a;
}
