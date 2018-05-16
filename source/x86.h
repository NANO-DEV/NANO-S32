// Architecture dependent routines

#ifndef _X86_H
#define _X86_H

static inline uint8_t inb(uint16_t port)
{
  uint8_t data;

  asm volatile("in %1,%0" : "=a" (data) : "d" (port));
  return data;
}

static inline void outb(uint16_t port, uint8_t data)
{
  asm volatile("out %0,%1" : : "a" (data), "d" (port));
}

static inline void stosb(void* addr, int data, size_t cnt)
{
  asm volatile("cld; rep stosb" :
               "=D" (addr), "=c" (cnt) :
               "0" (addr), "1" (cnt), "a" (data) :
               "memory", "cc");
}

#endif // _X86_H
