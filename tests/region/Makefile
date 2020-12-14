FLAGS =                      \
	-g                   \
	-Wall                \
	-Wextra              \
	-I ../../include     \


TESTS =                      \
	test_simple          \
	test_with_context    \

LIBS =                       \
	-pthread             \

HEADERS =                                \
	../../include/region_allocator.h \

all: $(TESTS)

%: $(HEADERS) %.c
	gcc $(FLAGS) -o $@ $^ $(LIBS)


run: $(TESTS)
	../run.sh $^


clean:
	rm -rf $(TESTS)
