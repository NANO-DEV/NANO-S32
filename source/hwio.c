// Basic IO

#include "types.h"
#include "x86.h"
#include "hwio.h"
#include "ulib/ulib.h"
#include "kernel.h"

// Wait and get key
uint io_getkey()
{
  regs16_t regs;

  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x10 << 8);
  int32(0x16, &regs);

  uint k = regs.ax;
  const uint hk = (k & 0xFF00);
  if(k != 0) {
    if(hk==KEY_DEL    ||
       hk==KEY_END    ||
       hk==KEY_HOME   ||
       hk==KEY_INS    ||
       hk==KEY_PG_DN  ||
       hk==KEY_PG_UP  ||
       hk==KEY_PRT_SC ||
       hk==KEY_UP     ||
       hk==KEY_LEFT   ||
       hk==KEY_RIGHT  ||
       hk==KEY_DOWN   ||
       hk==KEY_F1     ||
       hk==KEY_F2     ||
       hk==KEY_F3     ||
       hk==KEY_F4     ||
       hk==KEY_F5     ||
       hk==KEY_F6     ||
       hk==KEY_F7     ||
       hk==KEY_F8     ||
       hk==KEY_F9     ||
       hk==KEY_F10    ||
       hk==KEY_F11    ||
       hk==KEY_F12) {
      k &= 0xFF00;
    } else {
      k &= 0x00FF;
    }
  }
  return k;
}

// Put char (serial port)
extern uint8_t serial_status;
void io_serial_putc(char c)
{
  // Check device available
  if(serial_status & 0x80) {
    return;
  }

  regs16_t regs;

  memset(&regs, 0, sizeof(regs));
  regs.dx = 0;
  regs.ax = (0x01 << 8) | c;
  int32(0x14, &regs);
  serial_status = (regs.ax & 0xFF00) >> 8;

  return;
}

#define AT_DEFAULT (AT_T_LGRAY|AT_B_BLACK)

// Put char (screen)
void io_vga_putc(char c, uint8_t attr)
{
  // Default attributes
  if(attr == 0) {
    attr = AT_DEFAULT;
  }

  // Replace tabs with spaces
  if(c == '\t') {
    c = ' ';
  }

  // Set attribute
	regs16_t regs;
  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x09 << 8);
  regs.bx = attr;
  regs.cx = 1;
  int32(0x10, &regs);

  // Teletype output char)
  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x0E << 8) | c;
  regs.bx = 0;
  int32(0x10, &regs);

  // Add \r after \n
	if(c=='\n')	{
    memset(&regs, 0, sizeof(regs));
		regs.ax = (0x0E << 8) | '\r';
    regs.bx = 0;
		int32(0x10, &regs);
	}

	return;
}

// Write a char to screen in specific position
void io_vga_putc_attr(uint x, uint y, char c, uint8_t attr)
{
  // Default attributes
  if(attr == 0) {
    attr = AT_DEFAULT;
  }

  // Replace tabs with spaces
  if(c == '\t' || c == '\n') {
    c = ' ';
  }

  // Set cursor
  io_vga_setcursorpos(x, y);

  // Draw char with attribute
	regs16_t regs;
  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x09 << 8) | c;
  regs.bx = attr;
  regs.cx = 1;
  int32(0x10, &regs);

	return;
}

// Clear screen
void io_vga_clear()
{
  regs16_t regs;

  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x02 << 8);
  regs.bx = 0;
  regs.dx = 0;
  int32(0x10, &regs);

  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x06 << 8) | 0x00;
  regs.bx = (AT_DEFAULT << 8);
  regs.cx = 0;
  regs.dx = (24 << 8) | 79;
  int32(0x10, &regs);
}

// Get screen cursor position
void io_vga_getcursorpos(uint* x, uint* y)
{
  regs16_t regs;

  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x03 << 8);
  regs.bx = 0;
  int32(0x10, &regs);

  *x = (regs.dx & 0xFF);
  *y = ((regs.dx & 0xFF00) >> 8);
}

// Set screen cursor position
void io_vga_setcursorpos(uint x, uint y)
{
  regs16_t regs;

  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x02 << 8);
  regs.bx = 0;
  regs.dx = ((y & 0xFF) << 8) | (x & 0xFF);
  int32(0x10, &regs);
}

// Show or hide screen cursor
void io_vga_showcursor(uint show)
{
  regs16_t regs;

  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x01 << 8) | 0x03;
  regs.cx = show ? 0x0607 : 0x2000;
  int32(0x10, &regs);
}


// Time related functions
static uint BCD_to_int(char BCD)
{
    uint h = (BCD >> 4) * 10;
    return h + (BCD & 0xF);
}

// Get system time
void io_getdatetime(time_t* t)
{
  regs16_t regs;

  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x02 << 8);
  int32(0x1A, &regs);
  t->hour   = BCD_to_int((regs.cx & 0xFF00) >> 8);
  t->minute = BCD_to_int(regs.cx & 0xFF);
  t->second = BCD_to_int((regs.dx & 0xFF00) >> 8);

  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x04 << 8);
  int32(0x1A, &regs);
  t->year   = BCD_to_int(regs.cx & 0xFF) + 2000;
  t->month  = BCD_to_int((regs.dx & 0xFF00) >> 8);
  t->day    = BCD_to_int(regs.dx & 0xFF);
}

// Get system miliseconds
static uint base_ticks = 0xFFFFFFFF;
uint io_gettimer()
{
  // Get system timer ticks
  regs16_t regs;
  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x00 << 8);
  int32(0x1A, &regs);

  uint ticks = regs.cx * 0x10 + regs.dx +
    (regs.ax & 0xFF) * 0x1800B0;

  // Initialize reference
  if(ticks < base_ticks) {
    base_ticks = ticks;
  }

  ticks -= base_ticks;
  return (ticks * 1000) / 182;
}


// We need this buffer to be inside a 64KB bound
// to avoid DMA error
uint8_t disk_buff[DISK_SECTOR_SIZE];
extern uint8_t system_hwdisk; // System disk hardware ID

// Disk id to disk index
static uint hwdisk_to_disk(uint hwdisk)
{
  uint index;
  for(index=0; index<MAX_DISK; index++) {
    if(disk_info[index].id == hwdisk) {
      return index;
    }
  }
  return ERROR_NOT_FOUND;
}

// Disk index to disk id
static uint disk_to_hwdisk(uint index)
{
  if(index < MAX_DISK) {
    return disk_info[index].id;
  }

  return ERROR_NOT_FOUND;
}

// Return 0 on success
static uint disk_reset(uint hwdisk)
{
  regs16_t regs;

  memset(&regs, 0, sizeof(regs));
  regs.ax = 0;
  regs.dx = hwdisk;
  int32(0x13, &regs);
  return (regs.eflags & X86_CF);
}

// Disk management
typedef struct {
  uint cylinder_num;
  uint head_num;
  uint sector_num;
} diskinfo_t;

// Return 0 on success
static uint disk_get_info(uint hwdisk, diskinfo_t* diskinfo)
{
  regs16_t regs;

  memset(&regs, 0, sizeof(regs));
  regs.ax = (0x08 << 8);
  regs.dx = hwdisk;
  regs.es = 0;
  regs.di = 0;
  int32(0x13, &regs);

  uint result = (regs.eflags & X86_CF);
  if(!result) {

    diskinfo->head_num = 1 + ((regs.dx & 0xFF00) >> 8);
    diskinfo->sector_num = (regs.cx & 0x3F);
    diskinfo->cylinder_num = 1 +
      (((regs.cx & 0xFF00) >> 8) | ((regs.cx & 0xC0) << 2));

  } else {

    diskinfo->head_num = 0;
    diskinfo->sector_num = 0;
    diskinfo->cylinder_num = 0;
  }

  return result;
}

// Initialize disks info
void io_disks_init_info()
{
  // Setup disk identifiers
  disk_info[0].id = 0x00; // Floppy disk 0
  strncpy(disk_info[0].name, "fd0", sizeof(disk_info[0].name));

  disk_info[1].id = 0x01; // Floppy disk 1
  strncpy(disk_info[1].name, "fd1", sizeof(disk_info[1].name));

  disk_info[2].id = 0x80; // Hard disk 0
  strncpy(disk_info[2].name, "hd0", sizeof(disk_info[2].name));

  disk_info[3].id = 0x81; // Hard disk 1
  strncpy(disk_info[3].name, "hd1", sizeof(disk_info[3].name));

  // Initialize hardware related disks info
	for(uint i=0; i<MAX_DISK; i++) {
    uint hwdisk = disk_info[i].id;

    // Try to get info
		diskinfo_t dinfo;
    uint result = disk_get_info(hwdisk, &dinfo);
		disk_info[i].sectors = dinfo.sector_num;
		disk_info[i].sides = dinfo.head_num;
		disk_info[i].cylinders = dinfo.cylinder_num;

    // Use retrieved data if success
    if(result == 0) {
      disk_info[i].size = (disk_info[i].sectors *
        disk_info[i].sides * disk_info[i].cylinders) /
        (1048576 / DISK_SECTOR_SIZE);

      disk_info[i].last_access = 0;

      debug_putstr("DISK (%2x : size=%u MB sect_per_track=%d, sides=%d, cylinders=%d)\n",
        hwdisk, disk_info[i].size, disk_info[i].sectors, disk_info[i].sides,
        disk_info[i].cylinders);

    } else {
      // Failed. Do not use this disk
      disk_info[i].sectors = 0;
      disk_info[i].sides = 0;
      disk_info[i].cylinders = 0;
      disk_info[i].size = 0;
      disk_info[i].last_access = 0;
    }
  }

  // Set system disk
  system_disk = hwdisk_to_disk(system_hwdisk);
}

typedef struct {
  uint cylinder;
  uint head;
  uint sector;
} chs_t;

static void lba_to_chs( uint lba, uint spt, uint nh, chs_t* chs)
{
  uint temp = lba / spt;
  chs->sector = 1 + (lba % spt);
  chs->head = temp % nh;
  chs->cylinder = temp / nh;
}

// Return 0 on success
static uint disk_read_sector(uint disk, uint sector, size_t n, void* buff)
{
  uint hwdisk = disk_to_hwdisk(disk);

  chs_t chs;
  lba_to_chs(sector, disk_info[disk].sectors,
    disk_info[disk].sides, &chs);

  regs16_t regs;
  uint result = 0;

  for(uint attempt=0; attempt<3; attempt++) {
    if(attempt>0) {
      result = disk_reset(hwdisk);
      if(result) {
        debug_putstr("io_disk_read_sector: error reseting disk\n");
        break;
      }
    }

    memset(&regs, 0, sizeof(regs));
    regs.ax = (0x02 << 8) | n;
    regs.cx =
      ((chs.cylinder & 0xFF) << 8) |
      (chs.sector & 0x3F) |
      ((chs.cylinder & 0x300) >> 2);

    regs.dx = (chs.head << 8) | hwdisk;
    regs.es = ((uint)buff) / 0x10000;
    regs.bx = ((uint)buff) % 0x10000;
    int32(0x13, &regs);

    result = (regs.eflags & X86_CF);
    if(!result) {
      break;
    }
    else {
      result = (regs.ax & 0xFF00) >> 8;
    }
  }

  return result;
}

// Read disk, specific block, offset and size
// Returns 0 on success, another value otherwise
uint io_disk_read(uint disk, uint sector, uint offset, size_t size, void* buff)
{
  uint n_sectors = 0;
  uint i = 0;
  uint result = 0;

  // Check params
  if(buff == 0) {
    debug_putstr("Read disk: bad buffer\n");
    return 1;
  }

  if(disk_info[disk].size == 0) {
    debug_putstr("Read disk: bad disk\n");
    return 1;
  }

  // Compute initial sector and offset
  sector += offset / DISK_SECTOR_SIZE;
  offset = offset % DISK_SECTOR_SIZE;

  // io_disk_read_sector can only read entire and aligned sectors.
  // If requested offset is unaligned to sectors, read an entire
  // sector and copy only requested bytes in buff
  if(offset) {
    result = disk_read_sector(disk, sector, 1, disk_buff);
    i = min(DISK_SECTOR_SIZE-offset, size);
    memcpy(buff, &disk_buff[offset], i);
    sector++;
    size -= i;
  }

  // Now read aligned an entire sectors
  n_sectors = size / DISK_SECTOR_SIZE;

  if(n_sectors && result == 0) {
    // Handle DMA access 64kb boundary. Read sector by sector
    for(; n_sectors > 0; n_sectors--) {
      result = disk_read_sector(disk, sector, 1, disk_buff);
      if(result != 0) {
        break;
      }
      memcpy(buff+i, disk_buff, DISK_SECTOR_SIZE);
      i += DISK_SECTOR_SIZE;
      sector++;
      size -= DISK_SECTOR_SIZE;
    }
  }

  // io_disk_read_sector can only read entire and aligned sectors.
  // If requested size exceeds entire sectors, read
  // an entire sector and copy only requested bytes
  if(size && result == 0) {
    result = disk_read_sector(disk, sector, 1, disk_buff);
    memcpy(buff+i, disk_buff, size);
  }

  if(result != 0) {
    debug_putstr("Read disk error (%x)\n", result);
  }

  return result;
}

// Returns 0 on success
static uint disk_write_sector(uint disk, uint sector, size_t n, const void* buff)
{
  uint hwdisk = disk_to_hwdisk(disk);

  chs_t chs;
  lba_to_chs(sector, disk_info[disk].sectors,
    disk_info[disk].sides, &chs);

  regs16_t regs;
  uint result = 0;

  for(uint attempt=0; attempt<3; attempt++) {
    if(attempt>0) {
      result = disk_reset(hwdisk);
      if(result) {
        debug_putstr("io_disk_write_sector: error reseting disk\n");
        break;
      }
    }

    memset(&regs, 0, sizeof(regs));
    regs.ax = (0x03 << 8) | n;
    regs.cx =
      ((chs.cylinder & 0xFF) << 8) |
      (chs.sector & 0x3F) |
      ((chs.cylinder & 0x300) >> 2);

    regs.dx = (chs.head << 8) | hwdisk;
    regs.es = ((uint)buff) / 0x10000;
    regs.bx = ((uint)buff) % 0x10000;
    int32(0x13, &regs);

    result = (regs.eflags & X86_CF);
    if(!result) {
      break;
    } else {
      result = (regs.ax & 0xFF00) >> 8;
    }
  }

  return result;
}

// Write disk, specific sector, offset and size
// Returns 0 on success, another value otherwise
uint io_disk_write(uint disk, uint sector, uint offset, size_t size, const void* buff)
{
  uint n_sectors = 0;
  uint i = 0;
  uint result = 0;

  // Check params
  if(buff == 0) {
    debug_putstr("Write disk: bad buffer\n");
    return 1;
  }

  if(disk_info[disk].size == 0) {
    debug_putstr("Write disk: bad disk\n");
    return 1;
  }

  // Compute initial sector and offset
  sector += offset / DISK_SECTOR_SIZE;
  offset = offset % DISK_SECTOR_SIZE;

  // io_disk_write_sector can only write entire and aligned sectors.
  // If requested offset is unaligned to sectors, read an entire
  // sector, overwrite requested bytes, and write it
  if(offset) {
    result += disk_read_sector(disk, sector, 1, disk_buff);
    i = min(DISK_SECTOR_SIZE-offset, size);
    memcpy(disk_buff+offset, buff, i);
    result += disk_write_sector(disk, sector, 1, disk_buff);
    sector++;
    size -= i;
  }

  // Now write aligned an entire sectors
  n_sectors = size / DISK_SECTOR_SIZE;

  if(n_sectors && result == 0) {
    // Handle DMA access 64kb boundary. Write sector by sector
    for(; n_sectors > 0; n_sectors--) {
      memcpy(disk_buff, buff+i, DISK_SECTOR_SIZE);
      result = disk_write_sector(disk, sector, 1, disk_buff);
      if(result != 0) {
        break;
      }
      i += DISK_SECTOR_SIZE;
      sector++;
      size -= DISK_SECTOR_SIZE;
    }
  }

  // io_disk_write_sector can only write entire and aligned sectors.
  // If requested size exceeds entire sectors, read
  // an entire sector, overwrite requested bytes, and write
  if(size && result == 0) {
    result += disk_read_sector(disk, sector, 1, disk_buff);
    memcpy(disk_buff, buff+i, size);
    result += disk_write_sector(disk, sector, 1, disk_buff);
  }

  if(result != 0) {
    debug_putstr("Write disk error (%x)\n", result);
  }

  return result;
}

// Power off system using APM
void apm_shutdown()
{
  // Disconnect any APM interface
  regs16_t regs;
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x5304;
  regs.bx = 0;
  int32(0x15, &regs);

  // Connect to real mode interface
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x5301;
  regs.bx = 0;
  int32(0x15, &regs);

  // Enable power management
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x5308;
  regs.bx = 0x0001;
  regs.cx = 0x0001;
  int32(0x15, &regs);

  // Power off
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x5307;
  regs.bx = 0x0001;
  regs.cx = 0x0003;
  int32(0x15, &regs);

  while(1) {

  }
}
