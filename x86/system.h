/******************************************************************************
*
*   SYSTEM header
*
*/
#ifndef __SYSTEM_HEADER_DEFINED__
#define __SYSTEM_HEADER_DEFINED__

#define save_task_context(c)        setjmp(c)
#define restore_task_context(c, v)  longjmp(c, v)

#endif /* __SYSTEM_HEADER_DEFINED__ */
