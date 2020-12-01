#if defined(WIN32) || defined(_WIN32) || defined (__WIN32__)
# include "config_windows.h"
#endif
#include <stdio.h>
#include <string.h>
#define FRAME_REALLOC
#include "frame_allocator.h"


DECLARE_FRAME_ALLOCATOR();


void cb(char* a)
{
    if (!a)
        printf("Null\n");
    else
        printf("  cleaning: '%s'\n", a);
}

int main(int argc, char** argv)
{
    printf("Test case: %s", argv[0]);
    if (argc > 1)
        printf(" %s", argv[1]);
    printf("\n");

    frame_allocator_init(4096);

    char* a = frame_malloc_with_cleanup(4, (void (*)(void*)) cb);
    strcpy(a, "foo");
    printf("  a=%s\n", a);
    char* b = frame_malloc_with_cleanup(6, (void (*)(void*)) cb);
    strcpy(b, "hello");
    a = frame_realloc_with_cleanup(a, 7);
    strcat(a, "bar");
    printf("  a=%s b=%s\n", a, b);

    frame_swap(true);

    a = frame_realloc_with_cleanup(a, 7);
    char* c = frame_malloc_with_cleanup(4, (void (*)(void*)) cb);
    strcpy(c, "xxx");
    c = frame_realloc_with_cleanup(c, 7);
    strcat(c, "yyy");
    printf("  a=%s b=%s c=%s\n", a, b, c);

    frame_swap(true);

    char* d = frame_malloc_with_cleanup(8, (void (*)(void*)) cb);
    strcpy(d, "lastone");
    c = frame_realloc_with_cleanup(c, 12);
    strcat(c, "<<<<");
    printf("  a=%s c=%s d=%s\n", a, c, d);

    frame_allocator_destroy();
}
