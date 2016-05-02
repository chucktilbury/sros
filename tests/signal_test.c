/*
*   test the task stuff
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "../kern.h"

TCB *sig_task;

void task1(char *str);
void task2(char *str);
void task3(char *str);
int task4(char *str);
void sig_1(void);
void sig_2(void);
void sig_3(void);

void task_main(CMDLINE *cl) {

    int i, finished = 0;
    TCB *tcb;

    printf("task_main()!\n");
    for(i = 0; i < cl->argc; i++) {
        printf("arg %d = %s\n", i, cl->argv[i]);
    }

    if((sig_task = task_create(task3, "sig_task",
                DEFAULT_STACK_SIZE,
                DEFAULT_HEAP_SIZE,
                200)) == NULL) {
        printf("cannot allocate task 3\n");
    }        
    yield();

    if((tcb = task_create(task1, "task1",
                DEFAULT_STACK_SIZE,
                DEFAULT_HEAP_SIZE,
                50)) == NULL) {
        printf("cannot allocate task 1\n");
    }        

    if((tcb = task_create(task2, "task2",
                DEFAULT_STACK_SIZE,
                DEFAULT_HEAP_SIZE,
                51)) == NULL) {
        printf("cannot allocate task 2\n");
    }

    if((tcb = task_create(task4, "task4",
                DEFAULT_STACK_SIZE,
                DEFAULT_HEAP_SIZE,
                53)) == NULL) {
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
        
        raise_signal(sig_task, 5);
        raise_signal(sig_task, 6);
        
        if(i >= 2)
            break;
        //show_run_queue();
        yield();
    }

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

        raise_signal(sig_task, 7);

        if(i >= 3)
            break;
        //show_run_queue();
        yield();
    }
}

void test_yield(void);
void task3(char *str) {

    int i = 0;
    TCB *tcb;

    task_start_critical();
    catch_signal(sig_1, 5);
    catch_signal(sig_2, 6);
    catch_signal(sig_3, 7);
    task_end_critical();

    printf("signals created.\n");
    
    tcb = get_current_task_tcb();
    printf("T3:%p:%d:%s:%d:%d\n", 
        tcb, 
        i++, 
        str, 
        task_get_priority(tcb),
        sys_check_stack(tcb));
    
    task_set_status(NULL, 50); //TASK_SUSPENDED);    
    //while(1) yield();
    test_yield();
    
    /* should never see this... */
    printf("task 3 exiting.\n");
}

int task4(char *str) {

    int i = 0;
    TCB *tcb;

    tcb = get_current_task_tcb();
    while(1) {
        printf("T4:%p:%d:%s:%d:%d\n", 
            tcb, 
            i++, 
            str, 
            task_get_priority(tcb),
            sys_check_stack(tcb));        
        
        /*  Kill task 4 */
        raise_signal(NULL, SIGNAL_KILL);

        if(i >= 5)
            break;
        //show_run_queue();
        yield();
    }
    
    return 0;
}

void sig_1(void) {

    printf("received signal 1\n");
}

void sig_2(void) {

    printf("received signal 2\n");
}

void sig_3(void) {

    printf("received signal 3\n");
}



