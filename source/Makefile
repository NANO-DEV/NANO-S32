# Makefile

BOOTDIR := boot/

CC = gcc
LD = ld
NASM = nasm

CFLAGS  := -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -m32 -Wextra -fno-omit-frame-pointer
LDFLAGS := -melf_i386 --oformat binary

all: $(BOOTDIR)boot.bin kernel.n32

$(BOOTDIR)boot.bin: $(BOOTDIR)boot.s
	$(NASM) -O0 -w+orphan-labels -f bin -o $@ $(BOOTDIR)boot.s

kernel.n32: load.o kernel.o terminal_kbd.o terminal_vga.o util.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x8000 -o $@ load.o kernel.o terminal_kbd.o terminal_vga.o util.o

load.o: load.S asm.h
	$(CC) $(CFLAGS) -o $@ -c load.S

kernel.o: kernel.h kernel.c terminal.h types.h
	$(CC) $(CFLAGS) -o $@ -c kernel.c

terminal_kbd.o: terminal_kbd.c x86.h types.h
	$(CC) $(CFLAGS) -o $@ -c terminal_kbd.c

terminal_vga.o: terminal_vga.c terminal.h util.h x86.h types.h
	$(CC) $(CFLAGS) -o $@ -c terminal_vga.c

util.o: util.c util.h x86.h types.h
	$(CC) $(CFLAGS) -o $@ -c util.c

clean:
	@find . -name "*.o" -type f -delete
	@find . -name "*.bin" -type f -delete
	@find . -name "*.n32" -type f -delete

.PHONY: all clean
