#if defined(WIN32) || defined(_WIN32) || defined (__WIN32__)
# include "config_windows.h"
#endif
#include <stdio.h>
#include "smart_ptr_allocator.h"


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

    int* a = smart_ptr_malloc_with_cleanup(sizeof(int), (void (*)(void*)) cb_a);
    *a = 1;
    printf("  a=%d\n", *a);
    int* b = smart_ptr_malloc_with_cleanup(sizeof(int), (void (*)(void*)) cb_b);
    *b = 2;
    *a = 3;
    printf("  a=%d, b=%d\n", *a, *b);
    int* c = smart_ptr_malloc_with_cleanup(sizeof(int), (void (*)(void*)) cb_c);
    *c = 4;
    *b = 5;
    printf("  b=%d, c=%d\n", *b, *c);
    smart_ptr_unref(a);
    smart_ptr_unref(b);
    smart_ptr_unref(c);
}
