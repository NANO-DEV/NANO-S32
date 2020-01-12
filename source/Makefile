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

programs: $(PROGDIR)test.bin $(PROGDIR)edit.bin $(PROGDIR)nas.bin $(PROGDIR)unet.bin

$(PROGDIR)%.bin: $(PROGDIR)%.c $(ULIBDIR)ulib.o
	$(CC) $(CFLAGS) -I. -o $(PROGDIR)$*.o -c $(PROGDIR)$*.c
	$(LD) $(LDFLAGS) -N -e main -Ttext 0x20000 -T binary.ld -o $@ $(PROGDIR)$*.o $(ULIBDIR)ulib.o

$(BOOTDIR)boot.bin: $(BOOTDIR)boot.s
	$(NASM) -O0 -w+orphan-labels -f bin -o $@ $(BOOTDIR)boot.s

kernel.n32: load.o kernel.o net.o hwio.o x86.o cli.o pci.o fs.o $(ULIBDIR)ulib.o
	$(LD) $(LDFLAGS) -N -Ttext 0x8000 -T binary.ld -o $@ load.o x86.o cli.o \
	hwio.o kernel.o pci.o fs.o net.o $(ULIBDIR)ulib.o

load.o: load.S
	$(CC) $(CFLAGS) -o $@ -c load.S

cli.o: cli.c cli.h kernel.h types.h hwio.h syscall.h $(ULIBDIR)ulib.h x86.h fs.h net.h
	$(CC) $(CFLAGS) -o $@ -c cli.c

pci.o: pci.c pci.h types.h $(ULIBDIR)ulib.h x86.h
	$(CC) $(CFLAGS) -o $@ -c pci.c

net.o: net.c net.h pci.h types.h $(ULIBDIR)ulib.h x86.h hwio.h
	$(CC) $(CFLAGS) -o $@ -c net.c

kernel.o: kernel.c kernel.h types.h hwio.h syscall.h $(ULIBDIR)ulib.h x86.h fs.h cli.h pci.h net.h
	$(CC) $(CFLAGS) -o $@ -c kernel.c

hwio.o: kernel.h hwio.c x86.h types.h $(ULIBDIR)ulib.h
	$(CC) $(CFLAGS) -o $@ -c hwio.c

fs.o: fs.c kernel.h fs.h types.h hwio.h $(ULIBDIR)ulib.h
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
