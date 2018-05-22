// Architecture dependent routines

#ifndef _X86_H
#define _X86_H

// Read byte from port
static inline uint8_t inb(uint16_t port)
{
  uint8_t data;

  asm volatile("in %1,%0" : "=a" (data) : "d" (port));
  return data;
}

// Write byte to port
static inline void outb(uint16_t port, uint8_t data)
{
  asm volatile("out %0,%1" : : "a" (data), "d" (port));
}

// Processor flags
#define X86_CF 0x01
#define X86_ZF 0x04

typedef struct __attribute__ ((packed)) {
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
} regs16_t;

// Perform a bios call switching to virtual 8086 mode
void int32(uint8_t intnum, regs16_t* regs);

// Install ISR
void install_ISR();

#endif // _X86_H
