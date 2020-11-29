# Single header thread safe frame allocator for C

Frame allocator allows efficient memory management without the
need to individually deallocate memory. In many use cases, the
lifetime of an allocation is known to end at certain point when
a new phase is started in the program. For example, in a game
when a player has finished a level we can start a new allocation
generation to load and initialize the next level. However, we
still probably want to access the data in the older allocation
generation to show live game scene while loading, and possibly
to copy some data to the newer generation. This is where a
frame allocator can help.

A frame allocator allocates a single memory region which holds
two banks. The size of the frame is the size of a bank. The bank
can be switched by one instruction. And a bank can be cleared
by one other instruction. There is no need to deallocate objects
individually. We can still access the objects allocated before
the switch, and allocate new memory. But we cannot access objects
allocated before the switch before the previous switch. Newer
objects have replaced them.

Many object types need to be finalized properly. Memory is just
one resource they use. For example, an open file must be closed
etc. Thus, this frame allocator adds an option to register a
clean up callback at the same time we allocate memory. Once
the bank is swapped the second time, the callback is run.

## Platform requirements

The library has been written to Linux/Posix but can be easily
ported to any system supporting compare and swap operation. Just
redefine `CAS(destp,origp,newval)` before including the library.
By default, it uses `atomic_compare_exchange_week` which requires
`stdatomic.h`. If you do not use threads you can define this macro
simply to be `1`.

## Installation

To use the library just include `frame_allocator.h`. There are
two example probrams included in the repository. To build them
on Linux, type `make`, and to run them, type `make run`.

## API

### DECLARE_FRAME_ALLOCATOR()

Use this macro to declare a global variable to hold the frame
allocator. We do not want to pass frame allocator to `frame_malloc`
function each time it is called, since we want to match the
signature of the original malloc. A program using the library
should declare the frame allocator once.

### int frame_allocator_init(size_t frame_size)

Initializes the frame allocator with the given size.
Note that the actual space needed is twice the
size of the frame size. In addition, frame needs
to have space for the frame structure (32 bytes on modern
platforms). If the allocator
could not be initialized, the function return non zero.
On success, `0` is returned.

### void frame_allocator_destroy()

Destroy the frame allocator. No more allocations are allowed
once this function is called.

### void* frame_malloc(size_t size)

Allocate space from the current frame. Returns `NULL`,
if the frame is full.

### void* frame_malloc0(size_t size)

Allocate space from the current frame. Returns `NULL`,
if the frame is full. The allocated memory is cleared.

### void* frame_malloc_with_cleanup(size_t size, void (*cleanup)(void*))

Allocate space from the current frame and register
a callback for clean up. Returns `NULL`, if the frame
is full. The clean up is called on the second call
to `frame_swap`. `frame_malloc_with_cleanup` always clears
the allocated memory before returning.

### void* frame_realloc(void* ptr, size_t size)

Reallocate space from the current frame. Returns `NULL`,
if the frame is full. The old content is copied to the new
location. Note that if you have registered a clean up
callback, use `frame_realloc_with_cleanup` instead.

To enable this function, `#define FRAME_REALLOC` before
including `frame_allocator.h`. With this feature enabled
each allocation takes four bytes more space. If `size`
is the same or less than the old size, and the bank
has not been switched, the old pointer is returned. If
bank has been switched, new copy is returned.

### void* frame_realloc_with_cleanup(void* ptr, size)

Rellocate space from the current frame and register
a callback for clean up. Returns `NULL`, if the frame
is full. If the bank has been switched, the clean up
is removed from the task list of the old bank
and the old content is copied to the newly allocated
area.

To enable this function, `#define FRAME_REALLOC` before
including `frame_allocator.h`. With this feature enabled
each allocation takes four bytes more space. If `size`
is the same or less than the old size, and the bank
has not been switched, the old pointer is returned. If
bank has been switched, new copy is returned.

### int frame_get_bank_by_ptr(void* ptr)

Returns the bank identifier (zero or one) from which
bank the pointer has been allocated. If the pointer
is not allocated using the frame allocator, -1 is
returned.

### void frame_swap(bool clear)

Swaps the current frame. Clears the frame which
will become the current frame if `clear` is `true`.
Note that there needs to be some time between
frame swaps to ensure that all threads have
been scheduled to finish `frame_malloc_with_cleanup`
if the thread's time slice happed to run out
between the two `CAS` operations. In addition, some
objects with clean up callbacks may have been allocated
but not fully constructed. Typically this is
not an issue, however, if the time between frame swaps
is in seconds and constructors finish in microseconds.

Use just one thread to master frame swapping.
The memory area allocated before two
previous swaps must no longer be accessed. For
example,

```
frame_swap(true);
int* a = frame_malloc(sizeof(int));
*a = 1; // ok
frame_swap(true);
int* b = frame_malloc(sizeof(int));
*b = 2; // ok
*a = 3; // ok
frame_swap(true);
int* c = frame_malloc(sizeof(int));
*c = 4; // ok
*b = 5; // ok
*a = 6; // NOT OK, var 'a' is "freed"
```

### Debug logging

If you want to disable log messages each time frame was swapped,
define `LOGGER_DEBUG(...)` macro before including the library to be
empty. By default, it uses `printf` which is declared in `stdio.h`.

## License

MIT License

Copyright (c) 2020 Jukka-Pekka Iivonen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
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


/* Define FRAME_REALLOC if you want to be able to reallocate
 * objects. */
#ifdef FRAME_REALLOC
#include <string.h>
# define REALLOC_HEADER_SIZE                        \
         (sizeof(unsigned))
# define SET_REALLOC_SIZE(ptr,size)                 \
         *((unsigned*)(ptr)) = (size)
# define GET_REALLOC_SIZE(ptr)                      \
         (*((unsigned*)((ptr) - sizeof(unsigned))))
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

/* Returns the bank identifier (zero or one) from which
 * bank the pointer has been allocated. If the pointer
 * is not allocated using the frame allocator, -1 is
 * returned.
 */
static inline int
frame_get_bank_by_ptr(void* ptr)
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
        if (elem->cb)
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
frame_malloc0(size_t size)
{
    void* p = frame_malloc(size);

    if (!p)
        return NULL;

    BZERO(p, size);

    return p;
}

#ifdef FRAME_REALLOC

{
    unsigned old_size = GET_REALLOC_SIZE(ptr);

    if (frame_get_bank_by_ptr(ptr) == (int) GETBANK(_frame_allocator->fp) &&
        old_size >= size)
        return ptr;

    void* newp = frame_malloc(size);
    if (!newp)
        return NULL;

    memcpy(newp, ptr, old_size);

    return newp;
}

#endif

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
static inline void*
frame_realloc_with_cleanup(void* ptr, size_t size)
{
    int bank = GETBANK(_frame_allocator->fp);
    unsigned old_size = GET_REALLOC_SIZE(ptr);
    frame_clean_up_cb_list_t* e = NULL;

    if (frame_get_bank_by_ptr(ptr) == bank) {
        if (old_size >= size)
            return ptr;
    } else
        bank = !bank;

    frame_allocator_t* allocator = frame_allocator_get(bank);

    for (e = allocator->cleanups; e; e = e->next) {
        if (e->data == ptr)
            break;
    }

    if (!e)
        return NULL;

    void* newp = frame_malloc_with_cleanup(size, e->cb);

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
