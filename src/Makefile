ARCH := armv7

CFLAGS += -Wall -Wextra 

ifeq ($(ARCH),x86)
	CC = gcc
else ifeq ($(ARCH),armv7)
	CC = arm-none-eabi-gcc
	CFLAGS += -mthumb -march=armv7-m 
endif

COPTFLAGS := -Os

CFLAGS += $(COPTFLAGS)

all: picovm

debug: COPTFLAGS := -DDEBUG -g
debug: pico

picovm:
	$(CC) $(CFLAGS) -c -o picovm.o picovm.c

ifeq ($(ARCH),x86)
pico: picorun.o picovm.o picodisasm.o
	$(CC) picorun.o picovm.o picodisasm.o -o pico
else
pico:
endif

measure: picovm.o

.PHONY: clean
clean:
	rm -f pico *.d *.o core

