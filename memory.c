/*****************************************************************************\
*
*
*   Main heap manager.
*
*     Memory allocation is done on a heap basis.  Tasks have their own heap.
*   If the task spawns another task, then the new task's heap is allocated
*   out of global memory.  When a task is terminated, it's memory is freed
*   by the system.  A task's stack is allocated out of the task's heap, so
*   the heap needs to be allocated before the task's stack area.
*
*     The memory is kept in a single contiguous list.  The list has a format
*   that is flat and contiguous.  When a block is allocated, the list is
*   searched for the first unallocated block of memory that is large enough to
*   allocate the memory from.  
*
*   The free block could be split to allocate the memory required, or it could
*   be used as-is.  To justify a split, the block must be large enough to 
*   accomidate the free pointer as well as have enough memory to justify having 
*   a free space.  If a block is found that is large enough to allocate from, 
*   but not large enough to have another free block, then the extra memory is 
*   simply ignored. The threshold of how much memory is required for the 
*   allocater to allocate free space is a settable parameter.  I am thinking 
*   about 20 bytes is about right.  
*   
*   When a block is freed, the list is checked and all free blocks that are 
*   consecutive and free are merged together.
*
*   When the first block is allocated,
*       1.  the root block's size is adjusted to be the allocation size
*       2.  the status is changed to show that it is allocated
*       3.  another block header is created at the end of the allocated block
*
*     All memory block data structures have a "magic" number at the start to
*   help detect if there was a problem with over-shooting the end of a memory
*   block.
*
*    The heap is treated like an array of chars.  The next heap object is
*   indexed into the array instead of given a pointer to it.
*
*   typedef struct _hn_ {
*       UINT magic;     // magic number to detect over-runs
*       UCHAR status;   // allocated/free flag
*       UINT size;      // size of this heap block
*       UINT start;     // index of the start of this structure
*       UCHAR data[1];  // pointer returned to caller of alloc()
*   } HEAP_NODE;
*
*     Given the above data structure, a pointer to the next item in the heap is
*   given by "&heap[start+size]".
*
\*****************************************************************************/
#include "kern.h"

static HEAP *global_heap;

static int verify_node(HEAP *h, HCB *hcb);
static void inline clear_memory(void *ptr, UINT size);
static void *heap_alloc(HEAP *h, UINT size);
static void *heap_realloc(HEAP *h, void *ptr, UINT size);
static int heap_free(HEAP *h, void *ptr);

/******************************************************************************
*
*   Initialize the global heap.
*   
*/
int init_global_heap(UCHAR *start, UINT size) {

    /*  Since this is intended for embedded use, the amount of memory should
        be known in advance.  Therefore, this call should never really fail.
        The logic to catch a failure is here to aid in development only.  
        See main() in the file task.c to see how this funciton is used.  */
    if((global_heap = init_heap(start, size)) != NULL) 
        return TASK_SUCCESS;
    else
        return TASK_ERROR;
}


/******************************************************************************
*
*   Create a new heap from the specified pointer and size.  The pointer 
*   given to this function should then be cast to the heap HCB pointer 
*   to gain access to the first HCB.
*
*   If this function is successful, then return a pointer to the first heap
*   control block.  If it fails, then return NULL.
*
*/
HEAP *init_heap(UCHAR *start, UINT size) {

    HCB *hcb;
    HEAP *heap;

    /*  Is the caller sane???? */
    if((heap = (HEAP *)start) == NULL)
        return NULL;

    /*  Is the caller sane??? */
    if(size < HEAP_MIN_SIZE)
        return NULL;

    /* init the heap control block */
    heap->size = size;
    heap->address = (UINT)start;
    hcb = (HCB *)(start + sizeof(HEAP));

    /* init the first heap node */
    hcb->size = size - sizeof(HEAP);
    hcb->start = sizeof(HEAP);
    hcb->magic = HEAP_MAGIC;
    hcb->status = HEAP_STATUS_FREE;

    /*  Return a pointer to the new heap.  */
    return heap;
}

/******************************************************************************
*
*   The following functions are convience functions that hide the gory details 
*   of what they are doing.  They also allow the scope of the global heap
*   pointer to be local to this module.
*
*/
/******************************************************************************
*
*   Allocate memory from the global heap.
*
*/
void *global_alloc(UINT size) {

    return heap_alloc(global_heap, size);
}


/******************************************************************************
*
*   Reallocate memory from the global heap.
*
*/
void *global_realloc(void *ptr, UINT size) {

    return heap_realloc(global_heap, ptr, size);
}


/******************************************************************************
*
*   Free memory from the global heap.
*
*/
int global_free(void *ptr) {

    return heap_free(global_heap, ptr);
}


/******************************************************************************
*
*   Allocate memory from the current task's heap.
*
*/
void *task_alloc(UINT size) {

    TCB *tcb;
    
    tcb = get_current_task_tcb();
    return heap_alloc(tcb->heap, size);
}


/******************************************************************************
*
*   Reallocate memory from the current task's heap.
*
*/
void *task_realloc(void *ptr, UINT size) {

    TCB *tcb;
    
    tcb = get_current_task_tcb();
    return heap_realloc(tcb->heap, ptr, size);
}


/******************************************************************************
*
*   Free memory from the current task's heap.
*
*/
int task_free(void *ptr) {

    TCB *tcb;
    
    tcb = get_current_task_tcb();
    return heap_free(tcb->heap, ptr);
}


/******************************************************************************
*
*   Allocate memory from the specified task's heap.
*
*/
void *tcb_alloc(TCB *tcb, UINT size) {

    return heap_alloc(tcb->heap, size);
}


/******************************************************************************
*
*   Reallocate memory from the specified task's heap.
*
*/
void *tcb_realloc(TCB *tcb, void *ptr, UINT size) {

    return heap_realloc(tcb->heap, ptr, size);
}


/******************************************************************************
*
*   Free memory from the specified task's heap.
*
*/
int tcb_free(TCB *tcb, void *ptr) {

    return heap_free(tcb->heap, ptr);
}


/******************************************************************************
*
*   Walk the heap, verifying that all of the nodes in it are valid.  The 
*   If it finds an invalid node, then return a non-zero error code.  Otherwise 
*   return zero.
*
*/
int heap_walk(HEAP *h) {

    UINT max_addr;
    HCB *hcb;
    HEAP *heap;

    /* do some sanity checking */
    if((heap = h) == NULL)
        return 1;

    if(heap->size < HEAP_MIN_SIZE)
        return 2;

    if((UINT)heap != (UINT)heap->address)
        return 3;

    /* calculate the highest address that is in the heap */
    max_addr = (UINT)heap + heap->size;
    for(hcb = (HCB *)((UINT)heap + sizeof(HEAP));
                (UINT)hcb < max_addr;
                (UINT)hcb += hcb->size) {

        /* check the node */
        if(verify_node(heap, hcb))
            return 4;
    }

    return 0;
}


/******************************************************************************
*
*   Verify that the node that was allocated is actually a part of the
*   specified heap.  If the magic number matches the system magic number and
*   the ((start index + address of the heap) == the address of the heap node)
*   then the node is valid.  Otherwise it is not.  If the node is not valid,
*   then return a non-zero error code.  Otherwise return zero.
*/
int heap_verify_node(HEAP *h, void *node) {

    return verify_node(h, HEAP_PTR_TO_HCB(node));
}


/******************************************************************************
*
*   Static functions
*/
/******************************************************************************
*
*   Allocate memory from the heap control block and return a pointer to it.
*   The HCB parameter is the first heap node.  If there is no room, then
*   return NULL.
*/
static void *heap_alloc(HEAP *h, UINT size) {

    HEAP *heap;
    HCB *hcb, *nhcb;
    UINT max_addr;
    void *ptr;

    /* do some sanity checking */
    if((heap = h) == NULL)
        return NULL;

    if((UINT)heap != (UINT)heap->address)
        return NULL;

    /* find a node of sufficient size */
    /* calculate the highest address that is in the heap */
    max_addr = (UINT)heap + heap->size;
    for(hcb = (HCB *)((UINT)heap + sizeof(HEAP));
                (UINT)hcb < max_addr;
                (UINT)hcb += hcb->size) {

        /* This is clearly impossable.  If it happens, then there was a
            catastrophic error some where.  In any case, fail to allocate
            the memory. */
        if(hcb->size == 0 || hcb->status == 0) {
            return NULL;
        }

        /* if it is free then we could be interested in it */
        if(hcb->status == HEAP_STATUS_FREE) {
            /* check the size */
            if(hcb->size >= size + sizeof(HCB)) {
                break;  /* we have a winner.... */
            }

        }
    }

    /* did we find one? */
    if((UINT)hcb >= max_addr)  {
        /* nope... return an error */
        return NULL;
    }

    /* change the status of the node */
    hcb->status = HEAP_STATUS_USED;

    /* allocate from it */

    /* we have a node with enough room in it.  Do we want to split it or just
        ignore the left over space? */
    if(hcb->size > (size + sizeof(HCB) + HEAP_MIN_NODE_SIZE)) {
        /* split it */
        nhcb = (HCB *)((UINT)hcb + size + sizeof(HCB));
        nhcb->magic = HEAP_MAGIC;
        nhcb->status = HEAP_STATUS_FREE;
        nhcb->start = hcb->start + size + sizeof(HCB);
        nhcb->size = hcb->size - (size + sizeof(HCB));
        hcb->size = size + sizeof(HCB);
    }
    /* else we are done */

    /* clear the memory... */
    ptr =  HEAP_HCB_TO_PTR(hcb);
    clear_memory(ptr, size);

    /* return sucessfully created pointer */
    return ptr;
}


/******************************************************************************
*
*   Reallocate a memory block that was previously allocated by alloc().  The
*   HCB parameter must be the first node in the heap.  If the memory block was
*   allocated by alloc() and there is room for the new memory size, then a new
*   (or the same) pointer is returned.  Otherwise, there was an error and NULL
*   is returned.
*/
static void *heap_realloc(HEAP *h, void *ptr, UINT size) {

/*  TODO: Implemint this function.... */
    /* check this node to see if it is large enough */

    /* return sucessfully created pointer */
    return NULL; /*HEAP_HCB_TO_PTR(hcb);*/
}


/******************************************************************************
*
*   Free a memory block allocated by alloc() or realloc() and recombine free 
*   data areas.  If there is no error, then return 0. Otherwise return a 
*   non-zero error code.
*/
static int heap_free(HEAP *h, void *ptr) {

    HCB *hcb, *fhcb = NULL;
    UINT max_addr;
    int free_flag = 0;

    hcb = HEAP_PTR_TO_HCB(ptr);

    /* mark this one as free */
    hcb->status = HEAP_STATUS_FREE;

    /* walk the list and merge all of the free items */
    max_addr = (UINT)h + h->size;
    for(hcb = (HCB *)((UINT)h + sizeof(HEAP));
                (UINT)hcb < max_addr;
                (UINT)hcb += hcb->size) {

        /* if it is free then we could be interested in it */
        if(hcb->status == HEAP_STATUS_FREE) {
            /* are we in the state of merging blocks? */
            if(free_flag) {
                /* when we reach here, we have already encountered a free
                    block that we can merge this one with. */
                fhcb->size += hcb->size;
            }
            else {
                /* no so go there */
                free_flag = 1;
                fhcb = hcb;
            }
        }
        else
            free_flag = 0;
    }


    return 0;
}


/******************************************************************************
*
*   Verify the node using a HCB.
*/
static int verify_node(HEAP *h, HCB *hcb) {

    /* check the magic number */
    if(hcb->magic != HEAP_MAGIC)
        return 1;
    /* check the address */
    if(hcb->start != (UINT)(h)-(UINT)(hcb))
        return 2;

    /* no error */
    return 0;
}

/******************************************************************************
*
*   Clear a block of memory using 32 bit words and then clear the remainder
*   using bytes.  This funciton should be inlined if possable, to make it
*   as fast as possable.
*
*/
static void inline clear_memory(void *ptr, UINT size) {

    UINT *wbuf;
    UCHAR *cbuf;
    UINT idx, maxword, remainder;

    maxword = size / sizeof(UINT);
    remainder = size % sizeof(UINT);
    wbuf = (UINT *)ptr;
    cbuf = (UCHAR *)&wbuf[maxword];

    for(idx = 0; idx < maxword; idx++)
        wbuf[idx] = 0;

    for(idx = 0; idx < remainder; idx++)
        cbuf[idx] = 0;
}

/******************************************************************************
*
*   Print out the heap.  Debugging function only.
*/
#if 0
int print_heap(char *strg, HEAP *h) {

    UINT max_addr;
    HCB *hcb;
    HEAP *heap;

    fprintf(stdout, "\n%s", strg);
    /* do some sanity checking */
    if((heap = h) == NULL)
        return 1;
    else
        fprintf(stdout, "Heap Handle = 0x%08X\n", (UINT)h);

    if(heap->size < HEAP_MIN_SIZE)
        fprintf(stdout, "Heap size error:");
    
    fprintf(stdout, "  Heap size = 0x%08X\n", heap->size);

    if((UINT)heap != (UINT)heap->address)
        fprintf(stdout, "Heap address error:");
        
    fprintf(stdout, "  Heap address = 0x%08X\n", (UINT)heap->address);

    /* calculate the highest address that is in the heap */
    max_addr = (UINT)heap + heap->size;
    for(hcb = (HCB *)((UINT)heap + sizeof(HEAP));
                (UINT)hcb < max_addr;
                (UINT)hcb += hcb->size) {


        if(hcb->size == 0 || hcb->status == 0) {
            fprintf(stdout, "HEAP ERROR!\n");
            print_node(hcb);
            return 1;
        }

        /* check the node */
        print_node(hcb);
    }

    return 0;
}


/******************************************************************************
*
*   Print out the heap node.  Debugging function only.
*/
void print_node(HCB *hcb) {

    /* check the magic number */
    fprintf(stdout, "HCB handle = 0x%08X\n", (UINT)hcb);
    fprintf(stdout, "  hcb->magic  = 0X%08X\n", hcb->magic);
    fprintf(stdout, "  hcb->start  = 0X%08X\n", hcb->start);
    fprintf(stdout, "  hcb->size   = 0X%08X (%d)\n", hcb->size, hcb->size);
    fprintf(stdout, "  hcb->status = %s\n",
            (hcb->status == HEAP_STATUS_USED)? "ALLOCATED":
            (hcb->status == HEAP_STATUS_FREE)? "FREE": "ERROR");
}
#endif


