# Makefile

BOOTDIR := boot/
ULIBDIR := ulib/
PROGDIR := programs/

CC = gcc
LD = ld
NASM = nasm

CFLAGS  := -std=c99 -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -m32 -Wextra -fno-omit-frame-pointer -fno-stack-protector
LDFLAGS := -melf_i386 --oformat binary

all: $(BOOTDIR)boot.bin kernel.n32 programs

programs: $(PROGDIR)test.bin $(PROGDIR)edit.bin

$(PROGDIR)%.bin: $(PROGDIR)%.c $(ULIBDIR)ulib.o
	$(CC) $(CFLAGS) -I. -o $(PROGDIR)$*.o -c $(PROGDIR)$*.c
	$(LD) $(LDFLAGS) -s -N -e main -Ttext 0x20000 -o $@ $(PROGDIR)$*.o $(ULIBDIR)ulib.o

$(BOOTDIR)boot.bin: $(BOOTDIR)boot.s
	$(NASM) -O0 -w+orphan-labels -f bin -o $@ $(BOOTDIR)boot.s

kernel.n32: load.o kernel.o hwio.o fs.o x86.o $(ULIBDIR)ulib.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x8000 -o $@ load.o x86.o kernel.o hwio.o fs.o $(ULIBDIR)ulib.o

load.o: load.S
	$(CC) $(CFLAGS) -o $@ -c load.S

kernel.o: kernel.c kernel.h types.h hwio.h syscall.h $(ULIBDIR)ulib.h x86.h fs.h
	$(CC) $(CFLAGS) -o $@ -c kernel.c

hwio.o: hwio.c x86.h types.h $(ULIBDIR)ulib.h
	$(CC) $(CFLAGS) -o $@ -c hwio.c

fs.o: fs.c fs.h x86.h types.h hwio.h $(ULIBDIR)ulib.h
	$(CC) $(CFLAGS) -o $@ -c fs.c

$(ULIBDIR)ulib.o: $(ULIBDIR)ulib.c $(ULIBDIR)ulib.h syscall.h types.h
	$(CC) $(CFLAGS) -I. -o $@ -c $(ULIBDIR)ulib.c

x86.o: x86.s
	$(NASM) -O0 -w+orphan-labels -f aout -o $@ x86.s

clean:
	@find . -name "*.o" -type f -delete
	@find . -name "*.bin" -type f -delete
	@find . -name "*.n32" -type f -delete

.PHONY: all programs clean
