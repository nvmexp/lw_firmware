/*
 * Mock implementations of pvPortMallocFreeable() and vPortFreeFreeable() that
 * combines (coalescences) adjacent memory blocks as they are freed, and in so
 * doing limits memory fragmentation. Real implementations will be added once
 * we purchase heap_4.c
 */
#ifdef FREEABLE_HEAP

#include "OpenRTOS.h"
#include "task.h"

/* The application writer has already defined the array used for the RTOS
heap - probably so it can be placed in a special segment or address. */
extern unsigned int _freeable_heap_size;
extern unsigned portCHAR _freeable_heap[];

/*
 * Mock implementation of freeable heap malloc
 */
void *pvPortMallocFreeable( size_t xWantedSize )
{
    void *pvReturn =
        (void *)(_freeable_heap + _freeable_heap_size - _freeable_heap_size);

    return pvReturn;
}

/*
 * Mock implementation of freeable heap free
 */
void vPortFreeFreeable( void *pv )
{
    return;
}

#endif // FREEABLE_HEAP

