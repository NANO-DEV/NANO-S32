// Basic IO

#ifndef _IO_H
#define _IO_H

// Get key (wait)
uint io_getkey();

// VGA text mode
void io_vga_putc(char c, uint8_t attr);
void io_vga_putc_attr(uint x, uint y, char c, uint8_t attr);
void io_vga_clear();
void io_vga_getcursorpos(uint* x, uint* y);
void io_vga_setcursorpos(uint x, uint y);
void io_vga_showcursor(uint show);

// Put char serial
void io_serial_putc(char c);

// Get time
void io_getdatetime(time_t* time);
uint io_gettimer();

// Return 0 on success
#define DISK_SECTOR_SIZE 512 // hardware sector size in bytes
void io_disks_init_info();
uint io_disk_read(uint disk, uint sector, uint offset, size_t size, void* buff);
uint io_disk_write(uint disk, uint sector, uint offset, size_t size, const void* buff);


// Shutdown computer
void apm_shutdown();

#endif // _IO_H
