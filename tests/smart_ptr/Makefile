FLAGS =                      \
	-g                   \
	-Wall                \
	-Wextra              \
	-I ../../include     \


TESTS =                      \
	test_simple          \

LIBS =                       \
	-pthread             \

HEADERS =                                \
	../../include/smart_ptr_allocator.h \

all: $(TESTS)

%: $(HEADERS) %.c
	gcc $(FLAGS) -o $@ $^ $(LIBS)


run: $(TESTS)
	../run.sh $^


clean:
	rm -rf $(TESTS)
