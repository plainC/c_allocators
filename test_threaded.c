#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
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

void*
thread_cb(void* arg)
{
    int* is_running = arg;
    int counter = 0;

    while (*is_running) {
        int* a = frame_malloc(sizeof(int));
        if (!a) {
            printf("ALLOCATION ERROR\n");
            return NULL;
        }
        *a = counter++;
        usleep(4000);
        if (*a != counter-1) {
            printf("ERROR\n");
            return NULL;
        }
    }

    return NULL;
}

int main(int argc, char** argv)
{
    int nbr_of_threads = 2;

    printf("Test case: %s", argv[0]);
    if (argc > 1)
        printf(" with %d threads", (nbr_of_threads = atoi(argv[1])));
    printf("\n");

    if (frame_allocator_init(4096*1024)) {
        printf("Unable to allocate enough memory\n");
        exit(1);
    }

    pthread_t id[nbr_of_threads];
    int is_running = 1;
    for (int i=0; i < nbr_of_threads; i++)
        pthread_create(&id[i], NULL, thread_cb, &is_running);


    for (int i=0; i < 10; i++) {
        sleep(2);
        frame_swap(true);
    };
    is_running = 0;
    sleep(1);

    frame_allocator_destroy();
}
