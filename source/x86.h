// Architecture dependent routines

#ifndef _X86_H
#define _X86_H

// Read byte from port
static inline uint8_t inb(uint16_t port)
{
  uint8_t data;

  __asm__ volatile("in %1,%0" : "=a"(data) : "d"(port));
  return data;
}

// Read word from port
static inline uint16_t inw(uint16_t port)
{
  uint16_t data;

  __asm__ volatile("in %1,%0" : "=a"(data) : "d"(port));
  return data;
}

// Read double from port
static inline uint32_t ind(uint16_t port)
{
  uint32_t data;

  __asm__ volatile("in %1,%0" : "=a"(data) : "d"(port));
  return data;
}

// Write byte to port
static inline void outb(uint16_t port, uint8_t data)
{
  __asm__ volatile("out %0,%1" : : "a"(data), "d"(port));
}

// Write double to port
static inline void outd(uint16_t port, uint32_t data)
{
  __asm__ volatile("out %0,%1" : : "a"(data), "d"(port));
}

// Read array from port
static inline void insl(uint16_t port, void* addr, size_t cnt)
{
  __asm__ volatile("cld; rep insl" :
               "=D"(addr), "=c"(cnt) :
               "d"(port), "0"(addr), "1"(cnt) :
               "memory", "cc");
}

// Write array to port
static inline void outsl(uint16_t port, const void* addr, size_t cnt)
{
  __asm__ volatile("cld; rep outsl" :
               "=S"(addr), "=c"(cnt) :
               "d"(port), "0"(addr), "1"(cnt) :
               "cc");
}

// Get MSR
static inline void read_MSR(uint32_t msr, uint32_t* lo, uint32_t* hi)
{
  __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
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

// Dump registers in debug output
void dump_regs();

#endif // _X86_H
