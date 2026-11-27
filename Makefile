CC ?= cc
MPICC ?= mpicc
CPPFLAGS += -D_XOPEN_SOURCE=700
CFLAGS ?= -O2
CFLAGS += -std=c17 -Wall -Wextra -Wpedantic -ffp-contract=off
LDLIBS += -lpng -lm

COMMON = build/batch.o build/image.o

.PHONY: all clean
all: build/sequential build/distributed

build:
	mkdir -p $@

build/%.o: src/%.c src/batch.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/distributed.o: src/distributed.c src/batch.h | build
	$(MPICC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/sequential: build/sequential.o $(COMMON)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/distributed: build/distributed.o $(COMMON)
	$(MPICC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f build/*.o build/sequential build/distributed
