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
