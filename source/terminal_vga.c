// Terminal

#include "types.h"
#include "x86.h"
#include "util.h"
#include "terminal.h"

// Hardware text mode color constants
enum vga_color {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

const size_t VGA_WIDTH = 80;
const size_t VGA_HEIGHT = 25;
const uint8_t VGA_COLOR = (VGA_COLOR_LIGHT_GREY | VGA_COLOR_BLACK << 4);
const uint16_t VGA_PORT = 0x3D4;
uint16_t *const VGA_BUFFER = (uint16_t*)0xB8000;

#define BACKSPACE 0x100

void terminal_putentryat(char c, size_t x, size_t y)
{
	size_t index = y * VGA_WIDTH + x;
	VGA_BUFFER[index] = ((uint16_t) c | (uint16_t) VGA_COLOR << 8);
}

void terminal_putchar(int c)
{
	int pos;

  // Get cursor position
  outb(VGA_PORT, 14);
  pos = inb(VGA_PORT+1) << 8;
  outb(VGA_PORT, 15);
  pos |= inb(VGA_PORT+1);

	// Write char
	if(c == '\n') {
    pos += VGA_WIDTH - pos%VGA_WIDTH;
	} else if(c == BACKSPACE) {
    if(pos > 0) {
			--pos;
		}
  } else {
		terminal_putentryat(c, pos%VGA_WIDTH, pos/VGA_WIDTH);
		pos++;
	}

	// Scroll
	if((pos/VGA_WIDTH) >= VGA_HEIGHT) {
    memmove(VGA_BUFFER, VGA_BUFFER+VGA_WIDTH, sizeof(VGA_BUFFER[0])*(VGA_HEIGHT-1)*VGA_WIDTH);
    pos -= VGA_WIDTH;
    memset(VGA_BUFFER+pos, 0, sizeof(VGA_BUFFER[0])*(VGA_HEIGHT*VGA_WIDTH - pos));
  }

	// Move cursor
  outb(VGA_PORT, 14);
  outb(VGA_PORT+1, pos>>8);
  outb(VGA_PORT, 15);
  outb(VGA_PORT+1, pos);
  terminal_putentryat(' ', pos%VGA_WIDTH, pos/VGA_WIDTH);
}

void terminal_write(const char* data, size_t size)
{
	for(size_t i = 0; i < size; i++) {
		terminal_putchar(data[i]);
	}
}

void terminal_writestring(const char* data)
{
	terminal_write(data, strlen(data));
}
