// Hardware and basic IO

#ifndef _HWIO_H
#define _HWIO_H

// Get key press
enum IO_GETKEY_WAITMODE{
  IO_GETKEY_WAITMODE_NOWAIT = 0,
  IO_GETKEY_WAITMODE_WAIT
};
uint io_getkey(uint wait_mode);

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

// Init lapic, including timer
void lapic_init();
void lapic_eoi(); // Acknowledge
void set_network_IRQ(uint irq); // Set IRQ for network driver

// Shutdown computer
void apm_shutdown();

#endif // _HWIO_H
