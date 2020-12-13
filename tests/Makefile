FLAGS =                   \
	-g                \
	-Wall             \
	-Wextra           \
	-I ../include     \


TESTS =                   \
	test_simple       \
	test_threaded     \
	test_realloc      \
	test_with_context \
	test_keep         \

LIBS =                    \
	-pthread          \

HEADERS =                            \
	../include/frame_allocator.h \

all: $(TESTS)

%: $(HEADERS) %.c
	gcc $(FLAGS) -o $@ $^ $(LIBS)


run: $(TESTS)
	./run.sh $^


clean:
	rm -rf $(TESTS)
