#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "frame_allocator.h"


/* We run multiple threads which allocate memory to write integers.
 * We switch bank once per second. Each time bank is switched we
 * check that all allocation still holds the original value.
 */

#define USLEEP_BETWEEN_ALLOCS  10
#define TEST_LENGTH_IN_SECONDS 12
#define MAX_PTRS (10000000 / USLEEP_BETWEEN_ALLOCS)


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
    int bank = frame_get_bank_by_ptr(frame_malloc(4));
    int* ptrs[MAX_PTRS];

    while (*is_running) {
        int* a = frame_malloc(sizeof(int));
        if (!a) {
            printf("ALLOCATION ERROR\n");
            return NULL;
        }
        if (bank != frame_get_bank_by_ptr(a)) {
            for (int i=0; i < counter; i++)
                if ((*ptrs[i]) != i)
                    printf("ERROR: %d\n", i);
            printf("  Thread check: bank %d (%d) ok\n", bank, counter);
            counter = 0;
            bank = !bank;
        }
        ptrs[counter] = a;
        *a = counter++;
        usleep(USLEEP_BETWEEN_ALLOCS);
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


    for (int i=0; i < TEST_LENGTH_IN_SECONDS; i++) {
        sleep(1);
        frame_swap(true);
    };
    is_running = 0;
    sleep(1);

    frame_allocator_destroy();
}
