/*
* MIT License
*
* Copyright (c) 2020 Jukka-Pekka Iivonen
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef __FRAME_ALLOCATOR_H
#define __FRAME_ALLOCATOR_H


#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>


#ifndef LOGGER_DEBUG
# include <stdio.h>
# define LOGGER_DEBUG(...) printf(__VA_ARGS__)
#endif


/* Compare and swap method */
#ifndef CAS
#include <stdatomic.h>
#define CAS(destp,origp,newval)                                \
    atomic_compare_exchange_weak(destp,origp,newval)
#endif


/* bzero method */
#ifndef BZERO
#include <strings.h>
#define BZERO(p,size) bzero(p,size)
#endif


/* Tagged pointers */
#define BANK_1_TAG ((uintptr_t) 1)
#define UNTAG(ptr)                                             \
    ((unsigned char*) ( ((uintptr_t) (ptr)) & (~BANK_1_TAG) ))
#define GETBANK(ptr)  ( ((uintptr_t) (ptr)) &   BANK_1_TAG  )
#define SETBANK(ptr,bank)                                      \
    ((unsigned char*) ( ((uintptr_t) UNTAG(ptr)) | bank) )


/* We allow registering clean up callbacks to the frame */
typedef struct frame_clean_up_cb_list {
    void (*cb)(void*);
    void* data;
    struct frame_clean_up_cb_list* next;
} frame_clean_up_cb_list_t;


/* Frame allocator data type */
typedef struct {
    unsigned char* fp;
    unsigned char* start;
    size_t size;
    frame_clean_up_cb_list_t* cleanups;
} frame_allocator_t;


/* Use DECLARE_FRAME_ALLOCATOR() to declare frame allocator
 * in the source file */
#define DECLARE_FRAME_ALLOCATOR()                              \
    frame_allocator_t* _frame_allocator


extern DECLARE_FRAME_ALLOCATOR();


/* Get the frame allocator structure from the given bank.
 * Bank must be 0 or 1. */
static inline frame_allocator_t*
frame_allocator_get(int bank)
{
    return (frame_allocator_t*)
            (_frame_allocator->start +
            (_frame_allocator->size << bank) -
            sizeof(frame_allocator_t));
}

/* Initialize frame allocator with the given size.
 * Note that the actual space needed is twice the
 * size of the frame size. In addition, frame needs
 * to have space for the frame structure. */
static inline int
frame_allocator_init(size_t frame_size)
{
    frame_allocator_t* allocator;
    unsigned char* area = malloc(frame_size << 1);

    if (!area)
        return 1;

    /* Initialize bank 0 */
    allocator = (frame_allocator_t*)
            (area + frame_size - sizeof(frame_allocator_t));
    allocator->fp = (unsigned char*) allocator;
    allocator->start = area;
    allocator->size = frame_size;
    allocator->cleanups = NULL;

    /* Initialize bank 1 */
    allocator = (frame_allocator_t*)
            (area + (frame_size << 1) - sizeof(frame_allocator_t));
    allocator->fp = (unsigned char*) allocator;
    allocator->start = area;
    allocator->size = frame_size;
    allocator->cleanups = NULL;

    /* Activate bank 0 */
    _frame_allocator = (frame_allocator_t*)
            (area + frame_size - sizeof(frame_allocator_t));

    return 0;
}

/* Run the clean up callbacks registered for the frame
 */
static inline void
frame_allocator_clean_up(frame_allocator_t* allocator)
{
    for (frame_clean_up_cb_list_t* elem = allocator->cleanups;
         elem; elem = elem->next)
        elem->cb(elem->data);

    allocator->cleanups = NULL;
}

/* Destroy frame allocator. No more allocations are allowed
 * once this function is called.
 */
static inline void
frame_allocator_destroy()
{
    int bank = !GETBANK(_frame_allocator->fp);

    frame_allocator_t* allocator = frame_allocator_get(bank);
    frame_allocator_clean_up(allocator);
    allocator = frame_allocator_get(!bank);
    frame_allocator_clean_up(allocator);

    free(_frame_allocator->start);
}

/* Allocate space from the current frame. Returns NULL,
 * if the frame is full. */
static inline void*
frame_malloc(size_t size)
{
    unsigned char* orig;
    unsigned char* newp;

    do {
        orig = _frame_allocator->fp;
        newp = UNTAG(orig) - size;
        if (newp < _frame_allocator->start)
            return NULL;
    } while (!CAS(&_frame_allocator->fp,
                  &orig,
                  SETBANK(newp, GETBANK(orig))));

    return newp;
}

/* Allocate space from the current frame. Returns NULL,
 * if the frame is full. The memory is cleared. */
static inline void*
frame_malloc0(size_t size)
{
    void* p = frame_malloc(size);

    if (!p)
        return NULL;

    BZERO(p, size);

    return p;
}

/* Allocate space from the current frame and register
 * a callback for clean up. Returns NULL, if the frame
 * is full. */
static inline void*
frame_malloc_with_cleanup(size_t size, void (*cleanup)(void*))
{
    unsigned char* orig;
    unsigned char* newp;
    frame_clean_up_cb_list_t** cleanups;

    do {
        cleanups = &_frame_allocator->cleanups;
        orig = (unsigned char*) _frame_allocator->fp;
        newp = UNTAG(orig) - size - sizeof(frame_clean_up_cb_list_t);
        if (newp < _frame_allocator->start)
            return NULL;
    } while (!CAS(&_frame_allocator->fp,
                  &orig,
                  SETBANK(newp, GETBANK(orig))));

    frame_clean_up_cb_list_t* elem =
            (frame_clean_up_cb_list_t*) (newp + size);
    elem->cb = cleanup;
    elem->data = newp;
    BZERO(newp, size);
    do {
        elem->next = *cleanups;
    } while (!CAS(cleanups, &elem->next, elem));

    return newp;
}

/* Swaps the current frame. Clears the frame which
 * will become the current frame if 'clear' is true.
 * Note that there needs to be some time between
 * frame swaps to ensure that all threads have
 * been scheduled to finish frame_malloc_with_cleanup
 * if the thread's time slice happed to run out
 * between the two CAS operations. Typically this is
 * not an issue, however.
 *
 * Use just one thread to master frame swapping.
 * The memory area allocated before two
 * previous swaps must no longer be accessed. For
 * example,
 *
 * frame_swap(true);
 * int* a = frame_malloc(sizeof(int));
 * *a = 1; // ok
 * frame_swap(true);
 * int* b = frame_malloc(sizeof(int));
 * *b = 2; // ok
 * *a = 3; // ok
 * frame_swap(true);
 * int* c = frame_malloc(sizeof(int));
 * *c = 4; // ok
 * *b = 5; // ok
 * *a = 6; // NOT OK, var 'a' is "freed"
 */
static inline void
frame_swap(bool clear)
{
    int bank = !GETBANK(_frame_allocator->fp);

    LOGGER_DEBUG("Using bank: %d\n", bank);
    frame_allocator_t* allocator = frame_allocator_get(bank);

    if (clear) {
        frame_allocator_clean_up(allocator);
        allocator->fp = SETBANK(allocator, bank);
    }

    _frame_allocator = allocator;
}

#endif /* guard */
