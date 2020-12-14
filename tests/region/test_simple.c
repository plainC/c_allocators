#define REGION_REALLOC
#if defined(WIN32) || defined(_WIN32) || defined (__WIN32__)
# include "config_windows.h"
#endif
#include <stdio.h>
#include "region_allocator.h"


DECLARE_REGION_ALLOCATOR();

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

    region_allocator_init(4096);

    int* a = region_malloc_with_cleanup(sizeof(int), (void (*)(void*)) cb_a);
    *a = 1;
    printf("  a=%d\n", *a);
    int* b = region_malloc_with_cleanup(sizeof(int), (void (*)(void*)) cb_b);
    *b = 2;
    *a = 3;
    printf("  a=%d, b=%d\n", *a, *b);
    int* c = region_malloc_with_cleanup(sizeof(int), (void (*)(void*)) cb_c);
    *c = 4;
    *b = 5;
    printf("  b=%d, c=%d\n", *b, *c);
    region_allocator_clear();
    region_allocator_destroy();
}
