# Makefile

BOOTDIR := boot/
ULIBDIR := ulib/
PROGDIR := programs/

CC = gcc
LD = ld
NASM = nasm
CFLAGS := -std=c99 -fno-pic -static -fno-builtin -nostdinc -fno-strict-aliasing -O2 -Wall -m32 -MD -Wextra -fno-omit-frame-pointer -fno-stack-protector
LDFLAGS := -melf_i386 --oformat binary

KERNELOBJS = load.o x86.o cli.o hwio.o kernel.o pci.o fs.o net.o sound.o $(ULIBDIR)ulib.o

all: $(BOOTDIR)boot.bin kernel.n32 programs

programs: $(PROGDIR)play.bin $(PROGDIR)edit.bin $(PROGDIR)nas.bin $(PROGDIR)unet.bin

$(PROGDIR)%.bin: $(PROGDIR)%.c $(ULIBDIR)ulib.o $(ULIBDIR)ulib.h types.h
	$(CC) $(CFLAGS) -I. -o $(PROGDIR)$*.o -c $(PROGDIR)$*.c
	$(LD) $(LDFLAGS) -N -e main -Ttext 0x20000 -T binary.ld -o $@ $(PROGDIR)$*.o $(ULIBDIR)ulib.o

$(BOOTDIR)boot.bin: $(BOOTDIR)boot.s
	$(NASM) -O0 -w+orphan-labels -f bin -o $@ $(BOOTDIR)boot.s

-include *.d

kernel.n32: $(KERNELOBJS)
	$(LD) $(LDFLAGS) -N -Ttext 0x8000 -T binary.ld -o $@ $(KERNELOBJS)

load.o: load.S
	$(CC) $(CFLAGS) -o $@ -c load.S

$(ULIBDIR)ulib.o:
	$(CC) $(CFLAGS) -I. -o $@ -c $(ULIBDIR)ulib.c

x86.o: x86.s
	$(NASM) -O0 -w+orphan-labels -f elf -o $@ x86.s

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	@find . -name "*.o" -type f -delete
	@find . -name "*.d" -type f -delete
	@find . -name "*.bin" -type f -delete
	@find . -name "*.n32" -type f -delete

.PHONY: all programs clean
