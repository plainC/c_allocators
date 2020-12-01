FLAGS =         \
	-g      \
	-Wall   \
	-Wextra \

all: test_simple test_threaded test_realloc test_with_context test_keep


test_simple: frame_allocator.h test_simple.c
	gcc $(FLAGS) -o test_simple test_simple.c

test_threaded: frame_allocator.h test_threaded.c
	gcc $(FLAGS) -o test_threaded test_threaded.c -pthread

test_realloc: frame_allocator.h test_realloc.c
	gcc $(FLAGS) -o test_realloc test_realloc.c

test_keep: frame_allocator.h test_keep.c
	gcc $(FLAGS) -o test_keep test_keep.c

test_with_context: frame_allocator.h test_with_context.c
	gcc $(FLAGS) -o test_with_context test_with_context.c


run: all
#	./test_simple
#	./test_realloc
#	./test_threaded 8
	./test_keep
#	./test_with_context


clean:
	rm -rf test_simple test_realloc test_threaded test_with_context test_keep
