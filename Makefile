FLAGS =         \
	-Wall   \
	-Wextra \

all: test_simple


test_simple: frame_allocator.h test_simple.c
	gcc $(FLAGS) -o test_simple test_simple.c


run: all
	./test_simple


clean:
	rm -rf test_simple
