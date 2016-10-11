CC = gcc
CFLAGS = -std=gnu99 -Wall -g -pthread
OBJS = list.o threadpool.o main.o

.PHONY: all clean test

GIT_HOOKS := .git/hooks/pre-commit

all: $(GIT_HOOKS) sort

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

deps := $(OBJS:%.o=.%.o.d)
%.o: %.c
	$(CC) $(CFLAGS) -o $@ -MMD -MF .$@.d -c $<

sort: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -rdynamic

cache-test:
	perf stat --repeat 100 \
	-e cache-misses,cache-references,instructions,cycles \
	./sort 1 input.txt
	perf stat --repeat 100 \
	-e cache-misses,cache-references,instructions,cycles \
	./sort 2 input.txt
	perf stat --repeat 100 \
	-e cache-misses,cache-references,instructions,cycles \
	./sort 4 input.txt
	perf stat --repeat 100 \
	-e cache-misses,cache-references,instructions,cycles \
	./sort 8 input.txt

plot: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -rdynamic
	./sort 1 input.txt >> time.txt
	./sort 2 input.txt >> time.txt
	./sort 4 input.txt >> time.txt
	./sort 8 input.txt >> time.txt
	./sort 16 input.txt >> time.txt
	./sort 32 input.txt >> time.txt
	./sort 64 input.txt >> time.txt
	./sort 128 input.txt >> time.txt
	./sort 256 input.txt >> time.txt
	gnuplot scripts/plot.gp
	eog runtime.png

clean:
	rm -f $(OBJS) sort output.txt time.txt
	@rm -rf $(deps)

-include $(deps)
