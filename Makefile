FLAGS =         \
	-g      \
	-Wall   \
	-Wextra \

all: test_simple test_threaded test_realloc


test_simple: frame_allocator.h test_simple.c
	gcc $(FLAGS) -o test_simple test_simple.c

test_threaded: frame_allocator.h test_threaded.c
	gcc $(FLAGS) -o test_threaded test_threaded.c -pthread

test_realloc: frame_allocator.h test_realloc.c
	gcc $(FLAGS) -o test_realloc test_realloc.c


run: all
	./test_simple
	./test_realloc
	./test_threaded 8


clean:
	rm -rf test_simple test_realloc test_threaded
