SRCS=multipart.c
TEST_SRCS=multipart_test.c
CFLAGS=-Wall -Werror -Wextra -pedantic -fanalyzer -ggdb3
TARGET=main

# Default target
all: static test

target: $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Create a static library
static: $(SRCS)
	$(CC) $(CFLAGS) -c $(SRCS)
	ar rcs libmultipart.a multipart.o

# Create a test binary
test: $(TEST_SRCS) $(SRCS)
	$(CC) $(CFLAGS) -o test $(TEST_SRCS) $(SRCS)
	./test && rm -f test

clean:
	rm -f *.o *.a *.so $(TARGET) test