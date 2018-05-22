// Basic IO

#ifndef _IO_H
#define _IO_H

// Get key (wait)
uint io_getkey();

// VGA text mode
void io_vga_putc(char c);
void io_vga_getcursorpos(uint* x, uint* y);
void io_vga_setcursorpos(uint x, uint y);
void io_vga_showcursor(uint show);

// Put char serial
void io_serial_putc(char c);

// Get time
void io_gettime(time_t* time);

// Disk management
typedef struct {
  uint cylinder_num;
  uint head_num;
  uint sector_num;
} diskinfo_t;

// Return 0 on success
uint io_disk_get_info(uint disk, diskinfo_t* diskinfo);
uint io_disk_read_sector(uint disk, uint sector, size_t n, char* buff);
uint io_disk_write_sector(uint disk, uint sector, size_t n, const char* buff);

#endif // _IO_H
