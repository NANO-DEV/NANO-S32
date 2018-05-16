// Kernel

#include "types.h"
#include "kernel.h"
#include "terminal.h"


void kernel()
{
  terminal_writestring("nano32 kernel - boot\n");

	char pressed[2] = {0, 0};
  while(1) {
		pressed[0] = terminal_getkey();
		if( pressed[0] != -1 ) {
			terminal_writestring(pressed);
		}
  }
}
