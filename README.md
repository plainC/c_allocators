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

The libray also supports `frame_realloc(ptr,size)` if `FRAME_REALLOC`
macro is defined before inclusion. The size of the object is
then stored with the object, so each allocation takes four
bytes more space.

## Moving objects from the previous bank to the current bank

`frame_realloc` and `frame_realloc_with_callback` methods
can be used to copy objects from the previous bank to the new
one. Just switch the bank and call reallocation with the same
size to get a copy. If the object has a clean up callback, it is
moved as well to the current bank (note use
`frame_realloc_with_callback`). So, for instance, if a player
has picked up a sword in the previous level, the sword object
can be copied to the current level with a `realloc` call, and the
registered clean up is postponed by one bank swapping. So,
for example,

```
sword_t* sword = frame_malloc_with_callback(sizeof(sword_t), sword_destroy);
...
// this does not do anything since bank has not been swapped
sword = frame_realloc_with_callback(sword, sizeof(sword_t));
...
frame_swap();
...
// this takes a copy of the sword, and the clean up is postponed
sword = frame_realloc_with_callback(sword, sizeof(sword_t));
...
frame_swap();
...
// sword object still exists
player_use(sword);
...
frame_swap();
// sword has now been destroyed
```

## Passing explicit context instead of using a global variable

By default the frame allocator context is passed in a global
variable to save redundant stack usage and programmer's nerves.
It is possible to compile the library to use an explicit context
instead, however. This is done by defining
`#define FRAME_WITH_CONTEXT` before including `frame_allocator.h`.
The API is changed so that you must pass the context to all
functions. For `frame_allocator_init` and `frame_allocator_swap`
you must pass pointer to pointer. For instance,

```
frame_allocator_t* fa;
frame_allocator_init(&fa, 1024);
int* a = frame_malloc(fa, sizeof(int));
*a = 42;
frame_swap(&fa);
int* b = frame_malloc(fa, sizeof(int));
*b = *a + 1;
...
frame_allocator_destroy(fa);
```

## Platform requirements

The library has been written to Linux/Posix but has been tested
also in Windows. It can be easily ported to any system supporting
compare and swap operation. Just
redefine `CAS(destp,origp,newval)` before including the library.
By default, it uses `atomic_compare_exchange_week` which requires
`stdatomic.h`. If you do not use threads you can define this macro
simply to be `1` and compare and swap operation is not needed.

By default the library uses `bzero` and `strings.h` to clear
memory. This can be overriden by defining, for example,
`#define BZERO(ptr,n) SecureZeroMemory(ptr,n)` before including
the library.

By default the library uses `malloc` and `free` to allocate and
free the frame allocation area. To override them, for example,
define the following macros:

```
#define MALLOC(size) HeapAlloc(GetProcessHeap(), 0, size)
#define FREE(ptr) HeapFree(GetProcessHeap(), 0, ptr)
```


## Installation

### On Linux platform

To use the library just include `frame_allocator.h`. There are
four example programs included in the repository. To build them
on Linux, type `make`, and to run them, type `make run`.

### On Microsoft platform

The library can be configured externaly by defining a few macros
before including `frame_allocator.h`. See the code below.

```
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <Windows.h>

static inline bool
atomic_cas(void** object, void** expected, void* desired)
{
    bool success;

    if (sizeof(void*) == 4) {
        long* o = (long*) object;
        LONG d = (LONG) desired;
        long comp = (long) *expected;

        int32_t value = _InterlockedCompareExchange(o, d, comp);
        success = value == comp;
        if (!success)
            *expected = (void*) value;
    } else {
        int64_t* o = (int64_t*) object;
        int64_t d = (int64_t) desired;
        int64_t comp = (int64_t) *expected;

        int64_t value = _InterlockedCompareExchange64(o, d, comp);
        success = value == comp;
        if (!success)
            *expected = (void*) value;
    }

    return success;
}

#define CAS(object, expected, desired)      \
    atomic_cas(object, expected, desired)
#define BZERO(ptr,n)                        \
    SecureZeroMemory(ptr,n)

#include "frame_allocator.h"
```

A full example is given in `test_windows.c`. To compile with Microsoft
Compiler just type `cl test_windows.c`.

## API

### DECLARE_FRAME_ALLOCATOR()

Use this macro to declare a global variable to hold the frame
allocator. We do not want to pass frame allocator to `frame_malloc`
function each time it is called, since we want to match the
signature of the original malloc. A program using the library
should declare the frame allocator once.

### frame_allocator_init()

```
int frame_allocator_init(size_t frame_size)
```

Initializes the frame allocator with the given size.
Note that the actual space needed is twice the
size of the frame size. In addition, frame needs
to have space for the frame structure (32 bytes on modern
platforms). If the allocator
could not be initialized, the function return non zero.
On success, `0` is returned.

### frame_allocator_destroy()

```
void frame_allocator_destroy()
```

Destroy the frame allocator. No more allocations are allowed
once this function is called.

### frame_malloc()

```
void* frame_malloc(size_t size)
```

Allocate space from the current frame. Returns `NULL`,
if the frame is full.

### frame_malloc0()

```
void* frame_malloc0(size_t size)
```

Allocate space from the current frame. Returns `NULL`,
if the frame is full. The allocated memory is cleared.

### frame_malloc_with_cleanup()

```
void* frame_malloc_with_cleanup(size_t size, void (*cleanup)(void*))
```

Allocate space from the current frame and register
a callback for clean up. Returns `NULL`, if the frame
is full. The clean up is called on the second call
to `frame_swap`. `frame_malloc_with_cleanup` always clears
the allocated memory before returning.

### frame_realloc()

```
void* frame_realloc(void* ptr, size_t size)
```

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

### frame_realloc_with_cleanup()

```
void* frame_realloc_with_cleanup(void* ptr, size)
```

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

### frame_get_bank_by_ptr()

```
int frame_get_bank_by_ptr(void* ptr)
```

Returns the bank identifier (zero or one) from which
bank the pointer has been allocated. If the pointer
is not allocated using the frame allocator, -1 is
returned.

### frame_swap()

```
void frame_swap(bool clear)
```

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

Note that `frame_realloc` or `frame_realloc_with_callback`
methods should be used carefully to copy objects from
the previous bank to the current one in multi-threaded
applications. Best practise is to make all copies early
right after the bank swapping.

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


DECLARE_FRAME_ALLOCATOR();

void cb_a(int* a)
{
    printf("  Destroy a: %d\n", *a);
}

void cb_b(int* b)
{
    printf("  Destroy b: %d\n", *b);
}

void cb_c(int* c)
{
    printf("  Destroy c: %d\n", *c);
}

int main(int argc, char** argv)
{
    printf("Test case: %s", argv[0]);
    if (argc > 1)
        printf(" %s", argv[1]);
    printf("\n");

    frame_allocator_init(4096);

    int* a = frame_malloc_with_cleanup(sizeof(int), (void (*)(void*)) cb_a);
    *a = 1;
    printf("  a=%d\n", *a);
    frame_swap(true);
    int* b = frame_malloc_with_cleanup(sizeof(int), (void (*)(void*)) cb_b);
    *b = 2;
    *a = 3;
    printf("  a=%d, b=%d\n", *a, *b);
    frame_swap(true);
    int* c = frame_malloc_with_cleanup(sizeof(int), (void (*)(void*)) cb_c);
    *c = 4;
    *b = 5;
    printf("  b=%d, c=%d\n", *b, *c);
    frame_allocator_destroy();
}
