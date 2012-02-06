#ifndef _LOCK_FREE_RUNG_BUFFER_H_
#define _LOCK_FREE_RUNG_BUFFER_H_

/*
    Author: Brian Watling
    Email: brianwatling@hotmail.com
    Website: https://github.com/brianwatling
*/

#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <malloc.h>

#include "machine_specific.h"

typedef struct lockfree_ring_buffer
{
    //high and low are generally used together; no point putting them on separate cache lines
    volatile uint64_t high;
    char _cache_padding1[CACHE_SIZE - sizeof(uint64_t)];
    volatile uint64_t low;
    char _cache_padding2[CACHE_SIZE - sizeof(uint64_t)];
    size_t size;
    void* buffer[];
} lockfree_ring_buffer_t;

lockfree_ring_buffer_t* lockfree_ring_buffer_create(size_t size)
{
    assert(size);
    const size_t required_size = sizeof(lockfree_ring_buffer_t) + size * sizeof(void*);
    lockfree_ring_buffer_t* const ret = (lockfree_ring_buffer_t*)malloc(required_size);
    if(ret) {
        ret->high = 0;
        ret->low = 0;
        ret->size = size;
    }
    return ret;
}

void lockfree_ring_buffer_destroy(lockfree_ring_buffer_t* rb)
{
    free(rb);
}

void lockfree_ring_buffer_push(lockfree_ring_buffer_t* rb, void* in)
{
    assert(in);//can't store NULLs; we rely on a NULL to indicate a spot in the buffer has not been written yet

    while(1) {
        const uint64_t low = rb->low;
        load_load_barrier();//read low first; this means the buffer will appear larger or equal to its actual size
        const uint64_t high = rb->high;
        const uint64_t index = high % rb->size;
        if(!rb->buffer[index]
           && high - low <= rb->size
           && __sync_bool_compare_and_swap(&rb->high, high, high + 1)) {
            rb->buffer[index] = in;
            return;
        }
    }
}

void* lockfree_ring_buffer_pop(lockfree_ring_buffer_t* rb)
{
    while(1) {
        const uint64_t high = rb->high;
        load_load_barrier();//read high first; this means the buffer will appear smaller or equal to its actual size
        const uint64_t low = rb->low;
        const uint64_t index = low % rb->size;
        void* const ret = rb->buffer[index];
        if(ret
           && high > low
           && __sync_bool_compare_and_swap(&rb->low, low, low + 1)) {
            rb->buffer[index] = 0;
            return ret;
        }
    }
}

#endif
