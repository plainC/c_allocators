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
