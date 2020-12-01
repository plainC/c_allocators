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

void* copystr(char* s)
{
    printf("Copying '%s'\n", s);
    return frame_realloc_with_cleanup(s, GET_REALLOC_SIZE(s));
}

int
main(int argc, char** argv)
{
    printf("Test case: %s", argv[0]);
    if (argc > 1)
        printf(" %s", argv[1]);
    printf("\n");

    frame_allocator_init(4096);

    char* a = frame_malloc_with_cleanup(7, (void (*)(void*)) cb);
    printf("Keeping pointer: &a=%p\n", &a);
    frame_keep_ptr((void**) &a, (void*(*)(void*)) copystr);
    strcpy(a, "foobar");
    printf("  a=%s (%p)\n", a, a);
    char* b = frame_malloc_with_cleanup(6, (void (*)(void*)) cb);
    strcpy(b, "hello");
    printf("  a=%s b=%s\n", a, b);

    frame_swap(true);

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

    frame_swap(true);

    printf("  a=%s c=%s d=%s\n", a, c, d);

    printf("Discarding 'a'\n");
    if (0 != frame_discard_ptr((void**) &a))
        printf("Unable to discard 'a'\n");;

    frame_swap(true);

    frame_swap(true);

    frame_swap(true);

    frame_allocator_destroy();
}
