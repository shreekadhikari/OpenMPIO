CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g
LDLIBS = -lm

all: checkpoint_0 checkpoint_1 checkpoint_2

checkpoint_0: CFLAGS += -fopenmp
checkpoint_0: checkpoint_0.c

checkpoint_1: CFLAGS += -fopenmp
checkpoint_1: checkpoint_1.c

checkpoint_2: CFLAGS += -fopenmp
checkpoint_2: checkpoint_2.c

clean:
	$(RM) checkpoint_0 checkpoint_1 checkpoint_2