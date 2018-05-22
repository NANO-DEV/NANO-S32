// Basic IO

#include "types.h"
#include "x86.h"
#include "hwio.h"
#include "ulib/ulib.h"

uint io_getkey()
{
  regs16_t regs;

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

static uint serial_enabled = 1;
void io_serial_putc(char c)
{
  if(!serial_enabled) {
    return;
  }

  regs16_t regs;

  regs.dx = 0;
	regs.ax = (0x01 << 8) | c;
	int32(0x14, &regs);

  if(regs.ax & 0x8000) {
    serial_enabled = 0;
  }

  return;
}

void io_vga_putc(char c)
{
	regs16_t regs;

	if(c=='\t')	{
		regs.ax = (0x0E << 8) | ' ';
		regs.bx = 0x07;
		int32(0x10, &regs);
	} else {
		regs.ax = (0x0E << 8) | c;
		regs.bx = 0x07;
		int32(0x10, &regs);

		if(c=='\n')	{
			regs.ax = (0x0E << 8) | '\r';
			regs.bx = 0x07;
			int32(0x10, &regs);
		}
	}

	return;
}

void io_vga_getcursorpos(uint* x, uint* y)
{
  regs16_t regs;

  regs.ax = (0x03 << 8);
  regs.bx = 0;
  int32(0x10, &regs);

  *x = (regs.dx & 0xFF);
  *y = ((regs.dx & 0xFF00) >> 8);
}

void io_vga_setcursorpos(uint x, uint y)
{
  regs16_t regs;

  regs.ax = (0x02 << 8);
  regs.bx = 0;
  regs.dx = ((y & 0xFF) << 8) | (x & 0xFF);
  int32(0x10, &regs);
}

void io_vga_showcursor(uint show)
{
  regs16_t regs;

  regs.ax = (0x01 << 8) | 0x03;
  regs.cx = show ? 0x0607 : 0x2000;
  int32(0x10, &regs);
}

static uint BCD_to_int(char BCD)
{
    uint h = (BCD >> 4) * 10;
    return h + (BCD & 0xF);
}

void io_gettime(time_t* t)
{
  regs16_t regs;

  regs.ax = (0x02 << 8);
  int32(0x1A, &regs);
  t->hour   = BCD_to_int((regs.cx & 0xFF00) >> 8);
  t->minute = BCD_to_int(regs.cx & 0xFF);
  t->second = BCD_to_int((regs.dx & 0xFF00) >> 8);


  regs.ax = (0x04 << 8);
  int32(0x1A, &regs);
  t->year   = BCD_to_int(regs.cx & 0xFF) + 2000;
  t->month  = BCD_to_int((regs.dx & 0xFF00) >> 8);
  t->day    = BCD_to_int(regs.dx & 0xFF);
}

// Return 0 on success
static uint disk_reset(uint disk)
{
  regs16_t regs;

  regs.ax = 0;
  regs.dx = disk;
  int32(0x13, &regs);
  return (regs.eflags & X86_CF);
}

// Return 0 on success
uint io_disk_get_info(uint disk, diskinfo_t* diskinfo)
{
  regs16_t regs;

  regs.ax = (0x08 << 8);
  regs.dx = disk;
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
uint io_disk_read_sector(uint disk, uint sector, size_t n, char* buff)
{
  diskinfo_t diskinfo;
  uint result = io_disk_get_info(disk, &diskinfo);
  if(result) {
    debug_putstr("io_disk_read_sector: error getting disk info\n");
    return result;
  }

  chs_t chs;
  lba_to_chs( sector, diskinfo.sector_num, diskinfo.head_num, &chs);

  regs16_t regs;

  for(uint attempt=0; attempt<3; attempt++) {
    if(attempt>0) {
      result = disk_reset(disk);
      if(result) {
        debug_putstr("io_disk_read_sector: error reseting disk\n");
        break;
      }
    }

    regs.ax = (0x02 << 8) | n;
    regs.cx =
      ((chs.cylinder & 0xFF) << 8) |
      (chs.sector & 0x3F) |
      ((chs.cylinder & 0x300) >> 2);

    regs.dx = (chs.head << 8) | disk;
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

// Returns 0 on success
uint io_disk_write_sector(uint disk, uint sector, size_t n, const char* buff)
{
  diskinfo_t diskinfo;
  uint result = io_disk_get_info(disk, &diskinfo);
  if(result) {
    return result;
  }

  chs_t chs;
  lba_to_chs( sector, diskinfo.sector_num, diskinfo.cylinder_num, &chs);

  regs16_t regs;

  for(uint attempt=0; attempt<3; attempt++) {
    if(attempt>0) {
      result = disk_reset(disk);
      if(result) {
        debug_putstr("io_disk_write_sector: error reseting disk\n");
        break;
      }
    }

    regs.ax = (0x03 << 8) | n;
    regs.cx =
      ((chs.cylinder & 0xFF) << 8) |
      (chs.sector & 0x3F) |
      ((chs.cylinder & 0x300) >> 2);

    regs.dx = (chs.head << 8) | disk;
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
