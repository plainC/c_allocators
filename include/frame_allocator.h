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


#ifndef MALLOC
#define MALLOC(size) malloc(size)
#endif


#ifndef FREE
#define FREE(ptr) free(ptr)
#endif


#ifdef FRAME_WITH_CONTEXT
# define FRAME_CONTEXT_DECLAREP frame_allocator_t** _frame_allocator,
# define FRAME_CONTEXT_DECLAREV frame_allocator_t* _frame_allocator
# define FRAME_CONTEXT_DECLARE FRAME_CONTEXT_DECLAREV,
# define FRAME_CONTEXT _frame_allocator,
#else
# define FRAME_CONTEXT_DECLAREP
# define FRAME_CONTEXT_DECLAREV
# define FRAME_CONTEXT_DECLARE
# define FRAME_CONTEXT
#endif


/* Define FRAME_REALLOC if you want to be able to reallocate
 * objects. */
#ifdef FRAME_REALLOC
#include <string.h>
# define REALLOC_HEADER_SIZE                        \
         (sizeof(unsigned))
# define SET_REALLOC_SIZE(ptr,size)                 \
         *((unsigned*)(ptr)) = (size)
# define GET_REALLOC_SIZE(ptr)                      \
         (*((unsigned*)(((unsigned char*) (ptr)) - sizeof(unsigned))))
#else
# define REALLOC_HEADER_SIZE            0
# define SET_REALLOC_SIZE(ptr,size)     do {} while (0)
#endif


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

#ifdef FRAME_REALLOC
typedef struct frame_keep_list {
    void** ptrp;
    void* (*copy_func)(void*);
    struct frame_keep_list* next;
} frame_keep_list_t;
#endif

/* Frame allocator data type */
typedef struct {
    unsigned char* fp;
    unsigned char* start;
    size_t size;
    frame_clean_up_cb_list_t* cleanups;
#ifdef FRAME_REALLOC
    frame_keep_list_t* keeplist;
#endif
} frame_allocator_t;


#ifndef FRAME_WITH_CONTEXT
/* Use DECLARE_FRAME_ALLOCATOR() to declare frame allocator
 * in the source file */
#define DECLARE_FRAME_ALLOCATOR()                              \
    frame_allocator_t* _frame_allocator

extern DECLARE_FRAME_ALLOCATOR();
#endif


/* Get the frame allocator structure from the given bank.
 * Bank must be 0 or 1. */
static inline frame_allocator_t*
frame_allocator_get(FRAME_CONTEXT_DECLARE int bank)
{
    return (frame_allocator_t*)
            (_frame_allocator->start +
            (_frame_allocator->size << bank) -
            sizeof(frame_allocator_t));
}

/* Returns the bank identifier (zero or one) from which
 * bank the pointer has been allocated. If the pointer
 * is not allocated using the frame allocator, -1 is
 * returned.
 */
static inline int
frame_get_bank_by_ptr(FRAME_CONTEXT_DECLARE void* ptr)
{
    unsigned char* _p = (unsigned char*) ptr;

    if (_p < _frame_allocator->start ||
        _p >= _frame_allocator->start + (_frame_allocator->size << 1))
        return -1;

    if (_p < _frame_allocator->start + _frame_allocator->size)
        return 0;

    return 1;
}

/* Initialize frame allocator with the given size.
 * Note that the actual space needed is twice the
 * size of the frame size. In addition, frame needs
 * to have space for the frame structure. */
static inline int
frame_allocator_init(FRAME_CONTEXT_DECLAREP size_t frame_size)
{
    frame_allocator_t* allocator;
    unsigned char* area = MALLOC(frame_size << 1);

    if (!area)
        return 1;

    /* Initialize bank 0 */
    allocator = (frame_allocator_t*)
            (area + frame_size - sizeof(frame_allocator_t));
    allocator->fp = (unsigned char*) allocator;
    allocator->start = area;
    allocator->size = frame_size;
    allocator->cleanups = NULL;
#ifdef FRAME_REALLOC
    allocator->keeplist = NULL;
#endif

    /* Initialize bank 1 */
    allocator = (frame_allocator_t*)
            (area + (frame_size << 1) - sizeof(frame_allocator_t));
    allocator->fp = (unsigned char*) allocator;
    allocator->start = area;
    allocator->size = frame_size;
    allocator->cleanups = NULL;
#ifdef FRAME_REALLOC
    allocator->keeplist = NULL;
#endif

    /* Activate bank 0 */
#ifdef FRAME_WITH_CONTEXT
    *
#endif
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
        if (elem->cb)
            elem->cb(elem->data);

    allocator->cleanups = NULL;
}

/* Destroy frame allocator. No more allocations are allowed
 * once this function is called.
 */
static inline void
frame_allocator_destroy(FRAME_CONTEXT_DECLAREV)
{
    int bank = !GETBANK(_frame_allocator->fp);

    frame_allocator_t* allocator = frame_allocator_get(FRAME_CONTEXT bank);
    frame_allocator_clean_up(allocator);
    allocator = frame_allocator_get(FRAME_CONTEXT !bank);
    frame_allocator_clean_up(allocator);

#ifdef FRAME_REALLOC
    for (frame_keep_list_t *next, *e = _frame_allocator->keeplist; e; e = next) {
        next = e->next;
        FREE(e);
    }
#endif

    FREE(_frame_allocator->start);
}

/* Allocate space from the current frame. Returns NULL,
 * if the frame is full. */
static inline void*
frame_malloc(FRAME_CONTEXT_DECLARE size_t size)
{
    unsigned char* orig;
    unsigned char* newp;

    do {
        orig = _frame_allocator->fp;
        newp = UNTAG(orig) - size - REALLOC_HEADER_SIZE;
        if (newp < _frame_allocator->start)
            return NULL;
    } while (!CAS(&_frame_allocator->fp,
                  &orig,
                  SETBANK(newp, GETBANK(orig))));

    SET_REALLOC_SIZE(newp, size);

    return newp + REALLOC_HEADER_SIZE;
}

/* Allocate space from the current frame. Returns NULL,
 * if the frame is full. The memory is cleared. */
static inline void*
frame_malloc0(FRAME_CONTEXT_DECLARE size_t size)
{
    void* p = frame_malloc(FRAME_CONTEXT size);

    if (!p)
        return NULL;

    BZERO(p, size);

    return p;
}

#ifdef FRAME_REALLOC

/* Reallocate space from the current frame. Returns NULL,
 * if the frame is full. The old content is copied to new
 * location. Note that if you have registered a clean up
 * callback use fraem_realloc_with_cleanup instead.
 */
static inline void*
frame_realloc(FRAME_CONTEXT_DECLARE void* ptr, size_t size)
{
    unsigned old_size = GET_REALLOC_SIZE(ptr);

    if (frame_get_bank_by_ptr(FRAME_CONTEXT ptr) == (int) GETBANK(_frame_allocator->fp) &&
        old_size >= size)
        return ptr;

    void* newp = frame_malloc(FRAME_CONTEXT size);
    if (!newp)
        return NULL;

    memcpy(newp, ptr, old_size);

    return newp;
}

#endif

/* Allocate space from the current frame and register
 * a callback for clean up. Returns NULL, if the frame
 * is full. */
static inline void*
frame_malloc_with_cleanup(FRAME_CONTEXT_DECLARE size_t size, void (*cleanup)(void*))
{
    unsigned char* orig;
    unsigned char* newp;
    frame_clean_up_cb_list_t** cleanups;

    do {
        cleanups = &_frame_allocator->cleanups;
        orig = (unsigned char*) _frame_allocator->fp;
        newp = UNTAG(orig) - size - sizeof(frame_clean_up_cb_list_t)
               - REALLOC_HEADER_SIZE;
        if (newp < _frame_allocator->start)
            return NULL;
    } while (!CAS(&_frame_allocator->fp,
                  &orig,
                  SETBANK(newp, GETBANK(orig))));

    frame_clean_up_cb_list_t* elem =
            (frame_clean_up_cb_list_t*) (newp + size + REALLOC_HEADER_SIZE);
    elem->cb = cleanup;
    elem->data = newp + REALLOC_HEADER_SIZE;
    BZERO(newp + REALLOC_HEADER_SIZE, size);
    do {
        elem->next = *cleanups;
    } while (!CAS(cleanups, &elem->next, elem));

    SET_REALLOC_SIZE(newp, size);

    return newp + REALLOC_HEADER_SIZE;
}

#ifdef FRAME_REALLOC
/* Rellocate space from the current frame and register
 * a callback for clean up. Returns NULL, if the frame
 * is full. */
static inline void*
frame_realloc_with_cleanup(FRAME_CONTEXT_DECLARE void* ptr, size_t size)
{
    int bank = GETBANK(_frame_allocator->fp);
    unsigned old_size = GET_REALLOC_SIZE(ptr);
    frame_clean_up_cb_list_t* e = NULL;

    if (frame_get_bank_by_ptr(FRAME_CONTEXT ptr) == bank) {
        if (old_size >= size)
            return ptr;
    } else
        bank = !bank;

    frame_allocator_t* allocator = frame_allocator_get(FRAME_CONTEXT bank);

    for (e = allocator->cleanups; e; e = e->next) {
        if (e->data == ptr)
            break;
    }

    if (!e)
        return NULL;

    void* newp = frame_malloc_with_cleanup(FRAME_CONTEXT size, e->cb);

    if (!newp)
        return NULL;

    memcpy(newp, ptr, old_size);
    e->cb = NULL;
    e->data = NULL;

    return newp;
}

#endif

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
frame_swap(FRAME_CONTEXT_DECLAREP bool clear)
{
#ifdef FRAME_WITH_CONTEXT
    int bank = !GETBANK((*_frame_allocator)->fp);
    frame_allocator_t* allocator = frame_allocator_get(*_frame_allocator, bank);
#else
    int bank = !GETBANK(_frame_allocator->fp);
    frame_allocator_t* allocator = frame_allocator_get(FRAME_CONTEXT bank);
#endif

    LOGGER_DEBUG("Using bank: %d\n", bank);

#ifdef FRAME_REALLOC
    allocator->keeplist = frame_allocator_get(FRAME_CONTEXT !bank)->keeplist;
    /* Take copy of objects marked to be kept */
    frame_keep_list_t** prev = NULL;
    frame_keep_list_t* next;
    for (frame_keep_list_t* e = allocator->keeplist; e; e = next) {
        next = e->next;
        if (!e->ptrp) {
            if (prev == NULL)
                allocator->keeplist = next;
            else
                *prev = next;
            FREE(e);
        } else {
            *e->ptrp = e->copy_func ?
                    e->copy_func(*e->ptrp) :
                    frame_realloc(*e->ptrp, GET_REALLOC_SIZE(*e->ptrp));
           prev = &e->next;
       }
    }
#endif

    if (clear) {
        frame_allocator_clean_up(allocator);
        allocator->fp = SETBANK(allocator, bank);
    }

#ifdef FRAME_WITH_CONTEXT
    *
#endif
    _frame_allocator = allocator;
}

#ifdef FRAME_REALLOC
static inline int
frame_keep_ptr(void** ptrp, void* (*copy_func)(void*))
{
    frame_keep_list_t* elem = MALLOC(sizeof(frame_keep_list_t));
    frame_keep_list_t* list;

    if (!elem)
        return 1;

    elem->ptrp = ptrp;
    elem->copy_func = copy_func;
    do {
        list = _frame_allocator->keeplist;
        elem->next = list;
    } while (!CAS(&_frame_allocator->keeplist, &list, elem));

    return 0;
}

static inline int
frame_discard_ptr(void** ptrp)
{
    for (frame_keep_list_t* e = _frame_allocator->keeplist; e; e = e->next) {
        if (e->ptrp == ptrp)
            e->ptrp = NULL;
                return 0;
    }

    return 1;
}
#endif

#endif /* guard */
