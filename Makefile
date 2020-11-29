FLAGS =         \
	-Wall   \
	-Wextra \

all: test_simple test_threaded


test_simple: frame_allocator.h test_simple.c
	gcc $(FLAGS) -o test_simple test_simple.c

test_threaded: frame_allocator.h test_threaded.c
	gcc $(FLAGS) -o test_threaded test_threaded.c -pthread


run: all
	./test_simple
	./test_threaded 8


clean:
	rm -rf test_simple test_threaded
