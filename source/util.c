// Util general purpose functions

#include "types.h"
#include "x86.h"

size_t strlen(const char* str)
{
	size_t len = 0;
	while(str[len]) {
		len++;
	}
	return len;
}

void* memset(void* dst, int c, size_t n)
{
  stosb(dst, c, n);
  return dst;
}

void* memmove(void* vdst, void* vsrc, size_t n)
{
  char *dst, *src;

  dst = vdst;
  src = vsrc;
  while(n-- > 0) {
    *dst++ = *src++;
	}
  return vdst;
}
