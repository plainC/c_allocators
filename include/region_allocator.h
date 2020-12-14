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

#ifndef __REGION_ALLOCATOR_H
#define __REGION_ALLOCATOR_H


#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>


#ifndef MALLOC
#define MALLOC(size) malloc(size)
#endif


#ifndef FREE
#define FREE(ptr) free(ptr)
#endif


#ifdef REGION_WITH_CONTEXT
# define REGION_CONTEXT_DECLAREP region_allocator_t** _region_allocator,
# define REGION_CONTEXT_DECLAREV region_allocator_t* _region_allocator
# define REGION_CONTEXT_DECLARE REGION_CONTEXT_DECLAREV,
# define REGION_CONTEXT _region_allocator,
#else
# define REGION_CONTEXT_DECLAREP
# define REGION_CONTEXT_DECLAREV
# define REGION_CONTEXT_DECLARE
# define REGION_CONTEXT
#endif


/* Define REGION_REALLOC if you want to be able to reallocate
 * objects. */
#ifdef REGION_REALLOC
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


/* We allow registering clean up callbacks to the region */
typedef struct region_clean_up_cb_list {
    void (*cb)(void*);
    void* data;
    struct region_clean_up_cb_list* next;
} region_clean_up_cb_list_t;

/* Region allocator data type */
typedef struct {
    unsigned char* fp;
    unsigned char* start;
    size_t size;
    region_clean_up_cb_list_t* cleanups;
} region_allocator_t;


#ifndef REGION_WITH_CONTEXT
/* Use DECLARE_REGION_ALLOCATOR() to declare region allocator
 * in the source file */
#define DECLARE_REGION_ALLOCATOR()                              \
    region_allocator_t* _region_allocator

extern DECLARE_REGION_ALLOCATOR();
#endif


/* Initialize region allocator with the given size. */
static inline int
region_allocator_init(REGION_CONTEXT_DECLAREP size_t region_size)
{
    region_allocator_t* allocator;
    unsigned char* area = MALLOC(region_size);

    if (!area)
        return 1;

    /* Initialize bank 0 */
    allocator = (region_allocator_t*)
            (area + region_size - sizeof(region_allocator_t));
    allocator->fp = (unsigned char*) allocator;
    allocator->start = area;
    allocator->size = region_size;
    allocator->cleanups = NULL;

#ifdef REGION_WITH_CONTEXT
    *
#endif
    _region_allocator = allocator;

    return 0;
}

/* Run the clean up callbacks registered for the region
 */
static inline void
region_allocator_clean_up(region_allocator_t* allocator)
{
    for (region_clean_up_cb_list_t* elem = allocator->cleanups;
         elem; elem = elem->next)
        if (elem->cb)
            elem->cb(elem->data);

    allocator->cleanups = NULL;
}

/* Destroy region allocator. No more allocations are allowed
 * once this function is called.
 */
static inline void
region_allocator_destroy(REGION_CONTEXT_DECLAREV)
{
    region_allocator_clean_up(_region_allocator);

    FREE(_region_allocator->start);
}

/* Allocate space from the current region. Returns NULL,
 * if the region is full. */
static inline void*
region_malloc(REGION_CONTEXT_DECLARE size_t size)
{
    unsigned char* orig;
    unsigned char* newp;

    do {
        orig = _region_allocator->fp;
        newp = orig - size - REALLOC_HEADER_SIZE;
        if (newp < _region_allocator->start)
            return NULL;
    } while (!CAS(&_region_allocator->fp,
                  &orig,
                  newp));

    SET_REALLOC_SIZE(newp, size);

    return newp + REALLOC_HEADER_SIZE;
}

/* Allocate space from the current region. Returns NULL,
 * if the region is full. The memory is cleared. */
static inline void*
region_malloc0(REGION_CONTEXT_DECLARE size_t size)
{
    void* p = region_malloc(REGION_CONTEXT size);

    if (!p)
        return NULL;

    BZERO(p, size);

    return p;
}

#ifdef REGION_REALLOC

/* Reallocate space from the current region. Returns NULL,
 * if the region is full. The old content is copied to new
 * location. Note that if you have registered a clean up
 * callback use region_realloc_with_cleanup instead.
 */
static inline void*
region_realloc(REGION_CONTEXT_DECLARE void* ptr, size_t size)
{
    unsigned old_size = GET_REALLOC_SIZE(ptr);

    void* newp = region_malloc(REGION_CONTEXT size);
    if (!newp)
        return NULL;

    memcpy(newp, ptr, old_size);

    return newp;
}

#endif

/* Allocate space from the current region and register
 * a callback for clean up. Returns NULL, if the region
 * is full. */
static inline void*
region_malloc_with_cleanup(REGION_CONTEXT_DECLARE size_t size, void (*cleanup)(void*))
{
    unsigned char* orig;
    unsigned char* newp;
    region_clean_up_cb_list_t** cleanups;

    do {
        cleanups = &_region_allocator->cleanups;
        orig = (unsigned char*) _region_allocator->fp;
        newp = orig - size - sizeof(region_clean_up_cb_list_t)
               - REALLOC_HEADER_SIZE;
        if (newp < _region_allocator->start)
            return NULL;
    } while (!CAS(&_region_allocator->fp,
                  &orig,
                  newp));

    region_clean_up_cb_list_t* elem =
            (region_clean_up_cb_list_t*) (newp + size + REALLOC_HEADER_SIZE);
    elem->cb = cleanup;
    elem->data = newp + REALLOC_HEADER_SIZE;
    BZERO(newp + REALLOC_HEADER_SIZE, size);
    do {
        elem->next = *cleanups;
    } while (!CAS(cleanups, &elem->next, elem));

    SET_REALLOC_SIZE(newp, size);

    return newp + REALLOC_HEADER_SIZE;
}

#ifdef REGION_REALLOC
/* Rellocate space from the current region and register
 * a callback for clean up. Returns NULL, if the region
 * is full. */
static inline void*
region_realloc_with_cleanup(REGION_CONTEXT_DECLARE void* ptr, size_t size)
{
    unsigned old_size = GET_REALLOC_SIZE(ptr);
    region_clean_up_cb_list_t* e = NULL;

    if (old_size >= size)
        return ptr;

    for (e = _region_allocator->cleanups; e; e = e->next) {
        if (e->data == ptr)
            break;
    }

    if (!e)
        return NULL;

    void* newp = region_malloc_with_cleanup(REGION_CONTEXT size, e->cb);

    if (!newp)
        return NULL;

    memcpy(newp, ptr, old_size);
    e->cb = NULL;
    e->data = NULL;

    return newp;
}

#endif

static inline void
region_allocator_clear(REGION_CONTEXT_DECLAREV)
{
    region_allocator_clean_up(_region_allocator);
    _region_allocator->fp = _region_allocator->start;
}

#endif /* guard */
