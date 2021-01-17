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

#ifndef __SMART_PTR_ALLOCATOR_H
#define __SMART_PTR_ALLOCATOR_H


#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>


#ifndef MALLOC
#define MALLOC(size) malloc(size)
#endif


#ifndef FREE
#define FREE(ptr) free(ptr)
#endif


#define REFCOUNT_HEADER_SIZE                       \
         (sizeof(volatile unsigned))
#define GET_REFCOUNTP(ptr)                         \
         (((volatile unsigned*)(((unsigned char*) (ptr)) - sizeof(volatile unsigned))))
#define HAS_CLEAN_UP(ptr)                          \
    (*GET_REFCOUNTP(ptr) & 1)
#define GET_CLEAN_UP(ptr)                          \
    ((void (**)(void*)) (((unsigned char*) (ptr)) - sizeof(unsigned) - sizeof(void (*)(void*))))


#ifndef LOGGER_DEBUG
# include <stdio.h>
# define LOGGER_DEBUG(...) printf(__VA_ARGS__)
#endif


/* Compare and swap method */
#ifndef CAS_UINT
#include <stdatomic.h>
#define CAS_UINT(destp,origp,newval)                                \
    atomic_compare_exchange_weak(destp,origp,newval)
#endif


/* bzero method */
#ifndef BZERO
#include <strings.h>
#define BZERO(p,size) bzero(p,size)
#endif


static inline void*
smart_ptr_malloc(size_t size)
{
    void* p = MALLOC(size + sizeof(volatile unsigned));

    if (!p)
        return NULL;

    *((unsigned*) p) = 1 << 1;

    return (void*) (((unsigned char*) p) + sizeof(volatile unsigned));
}

static inline void*
smart_ptr_malloc0(size_t size)
{
    void* p = smart_ptr_malloc(size);

    if (!p)
        return NULL;

    BZERO(p, size);

    return p;
}

static inline void*
smart_ptr_malloc_with_cleanup(size_t size, void (*cleanup)(void*))
{
    void* p = MALLOC(size + sizeof(volatile unsigned) +
                     sizeof(void (*)(void*)));

    if (!p)
        return NULL;

    *((void (**)(void*)) p) = cleanup;
    unsigned char* q = ((unsigned char*) p) + sizeof(void (*)(void*));
    *((unsigned*) q) = (1 << 1) | 1;

    return (void*) (((unsigned char*) q) + sizeof(volatile unsigned));
}

static inline void*
smart_ptr_ref(void* p)
{
    unsigned refcount;

    do {
        refcount = *GET_REFCOUNTP(p);
        if (!(refcount >> 1))
            return NULL;
    } while (!CAS_UINT(GET_REFCOUNTP(p), &refcount, refcount + (1 << 1)));

    return p;
}

static inline void
smart_ptr_unref(void* p)
{
    unsigned refcount;

    do {
        refcount = *GET_REFCOUNTP(p);
        if (!(refcount >> 1))
            return;
    } while (!CAS_UINT(GET_REFCOUNTP(p), &refcount, refcount - (1 << 1)));

    if (((refcount - (1 << 1)) >> 1) == 0) {
        if (HAS_CLEAN_UP(p)) {
            (*GET_CLEAN_UP(p))(p);
            FREE((void*) GET_CLEAN_UP(p));
        } else
            FREE((void*) GET_REFCOUNTP(p));
    }
}

#endif /* guard */
