CC = gcc

CFLAGS = -O3

default: cpu.o cpu.h emulate.c
	$(CC) $(CFLAGS) cpu.o emulate.c -o A1Emu

cpu.o: cpu.c cpu.h
	$(CC) $(CFLAGS) -c cpu.c

ifeq ($(OS),Windows_NT)
clean:
	del A1Emu.exe cpu.o
else
clean:
	rm A1Emu cpu.o
endif

