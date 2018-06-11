// Basic IO

#include "types.h"
#include "x86.h"
#include "hwio.h"
#include "ulib/ulib.h"
#include "kernel.h"


// PC keyboard interface constants

#define KB_PORT_STATUS  0x64    // kbd controller status port(I)
#define KB_PORT_DATA    0x60    // kbd data port(I)
#define KB_DATA_IN_BUFF 0x01    // kbd data in buffer

#define NO              0

#define SHIFT           (1<<0)
#define CTL             (1<<1)
#define ALT             (1<<2)

#define CAPSLOCK        (1<<3)
#define NUMLOCK         (1<<4)
#define SCROLLLOCK      (1<<5)

#define E0ESC           (1<<6)

// C('A') == Control-A
#define C(x) (x - '@')

static const uint8_t shiftcode[256] =
{
  [0x1D] CTL,
  [0x2A] SHIFT,
  [0x36] SHIFT,
  [0x38] ALT,
  [0x9D] CTL,
  [0xB8] ALT
};

static const uint8_t togglecode[256] =
{
  [0x3A] CAPSLOCK,
  [0x45] NUMLOCK,
  [0x46] SCROLLLOCK
};

// Keyboard maps
static const uint8_t normalmap[256] =
{
  NO,   KEY_ESC, '1',  '2',  '3',  '4',  '5',  '6',  // 0x00
  '7',  '8',  '9',  '0',  '-',  '=',  KEY_BACKSPACE, KEY_TAB,
  'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',     // 0x10
  'o',  'p',  '[',  ']',  KEY_RETURN, NO,   'a',  's',
  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',     // 0x20
  '\'', '`',  NO,   '\\', 'z',  'x',  'c',  'v',
  'b',  'n',  'm',  ',',  '.',  '/',  NO,   '*',     // 0x30
  NO,   ' ',  NO, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
  KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,  NO,   NO,   '7',
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  NO,   NO,   NO,   KEY_F11, // 0x50
  KEY_F12,
  [0x9C] KEY_RETURN,// KP_Enter
  [0xB5] '/',       // KP_Div
  [0xC8] KEY_UP,    [0xD0] KEY_DOWN,
  [0xC9] KEY_PG_UP, [0xD1] KEY_PG_DN,
  [0xCB] KEY_LEFT,  [0xCD] KEY_RIGHT,
  [0x97] KEY_HOME,  [0xCF] KEY_END,
  [0xD2] KEY_INS,   [0xD3] KEY_DEL
};

static const uint8_t shiftmap[256] =
{
  NO,   033,  '!',  '@',  '#',  '$',  '%',  '^',  // 0x00
  '&',  '*',  '(',  ')',  '_',  '+',  KEY_BACKSPACE, KEY_TAB,
  'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10
  'O',  'P',  '{',  '}',  KEY_RETURN, NO,   'A',  'S',
  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20
  '"',  '~',  NO,   '|',  'Z',  'X',  'C',  'V',
  'B',  'N',  'M',  '<',  '>',  '?',  NO,   '*',  // 0x30
  NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
  NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',  // 0x40
  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
  '2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,   // 0x50
  [0x9C] KEY_RETURN, // KP_Enter
  [0xB5] '/',        // KP_Div
  [0xC8] KEY_UP,     [0xD0] KEY_DOWN,
  [0xC9] KEY_PG_UP,  [0xD1] KEY_PG_DN,
  [0xCB] KEY_LEFT,   [0xCD] KEY_RIGHT,
  [0x97] KEY_HOME,   [0xCF] KEY_END,
  [0xD2] KEY_INS,    [0xD3] KEY_DEL
};

static const uint8_t ctlmap[256] =
{
  NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
  NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
  C('Q'),  C('W'),  C('E'),  C('R'),  C('T'),  C('Y'),  C('U'),  C('I'),
  C('O'),  C('P'),  NO,      NO,      KEY_RETURN, NO,   C('A'),  C('S'),
  C('D'),  C('F'),  C('G'),  C('H'),  C('J'),  C('K'),  C('L'),  NO,
  NO,      NO,      NO,      C('\\'), C('Z'),  C('X'),  C('C'),  C('V'),
  C('B'),  C('N'),  C('M'),  NO,      NO,      C('/'),  NO,      NO,
  [0x9C] KEY_RETURN, // KP_Enter
  [0xB5] C('/'),     // KP_Div
  [0xC8] KEY_UP,     [0xD0] KEY_DOWN,
  [0xC9] KEY_PG_UP,  [0xD1] KEY_PG_DN,
  [0xCB] KEY_LEFT,   [0xCD] KEY_RIGHT,
  [0x97] KEY_HOME,   [0xCF] KEY_END,
  [0xD2] KEY_INS,    [0xD3] KEY_DEL
};

// Function to read keyboard
// Convert chars to ascii
// Special key codes also used
// Returns 0 if no key pressed
static uint8_t kb_get()
{
  static uint shift = NUMLOCK;
  static const uint8_t* charcode[4] = {
    normalmap, shiftmap, ctlmap, ctlmap
  };
  uint8_t st, data, c;

  // Check if there is data available
  st = inb(KB_PORT_STATUS);
  if((st & KB_DATA_IN_BUFF) == 0) {
    return 0;
  }

  // Read data
  data = inb(KB_PORT_DATA);

  // Skip mouse data
  if(st & 0x20) {
    return 0;
  }

  // Escape code. Return and skip next key
  if(data == 0xE0) {
    shift |= E0ESC;
    return 0;

  } else if(data & 0x80) {
    // Key released
    data = (shift & E0ESC ? data : data & 0x7F);
    shift &= ~(shiftcode[data] | E0ESC);
    return 0;

  } else if(shift & E0ESC) {
    // Last character was an E0 escape; or with 0x80
    data |= 0x80;
    shift &= ~E0ESC;
  }

  // Find key in map and translate
  shift |= shiftcode[data];
  shift ^= togglecode[data];
  c = charcode[shift & (CTL | SHIFT)][data];

  // Handle CAPSLOCK enabled
  if(shift & CAPSLOCK) {
    if('a' <= c && c <= 'z') {
      c += 'A' - 'a';
    } else if('A' <= c && c <= 'Z') {
      c += 'a' - 'A';
    }
  }

  return c;
}

// Wait until get key
uint io_getkey()
{
  uint k = 0;

  // Wait
  while(k == 0) {
    k = kb_get();
  }

  return k;
}

// Put char (serial port)
static uint8_t serial_status = 0xFF;
#define COM1_PORT 0x03F8
void io_serial_putc(char c)
{
  if(serial_status == 0xFF) {
    // Init serial
    outb(COM1_PORT + 1, 0x00);    // Disable all interrupts
    outb(COM1_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1_PORT + 1, 0x00);    //                  (hi byte)
    outb(COM1_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    serial_status = 1;

    // If status is 0xFF, no serial port
    if(inb(COM1_PORT+5) == 0xFF) {
      serial_status = 0;
    }

    debug_putstr("Serial port initialized\n");
  }

  // If serial port is active, put char
  if(serial_status == 1) {
    // Wait
    for(uint i=0; i<128 && !(inb(COM1_PORT+5) & 0x20); i++ ) {
    }
    outb(COM1_PORT, c);
  }
}

#define AT_DEFAULT (AT_T_LGRAY|AT_B_BLACK)
static const uint VGA_PORT = 0x03D4;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 28;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

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

  // Get cursor position: col + width*row
  uint pos = 0;
  outb(VGA_PORT, 14);
  pos = inb(VGA_PORT+1) << 8;
  outb(VGA_PORT, 15);
  pos |= inb(VGA_PORT+1);

  // Set char
  if(c == '\n') {
    pos += VGA_WIDTH - pos%VGA_WIDTH;
  } else {
    VGA_MEMORY[pos++] = (c & 0xFF) | (attr << 8);
  }

  // Scroll
  if((pos/VGA_WIDTH) > (VGA_HEIGHT-1)) {

    memcpy(VGA_MEMORY, VGA_MEMORY+VGA_WIDTH,
      sizeof(VGA_MEMORY[0])*(VGA_HEIGHT-1)*VGA_WIDTH);

    pos -= VGA_WIDTH;

    const uint16_t empty = ' '|(AT_DEFAULT<<8);
    for(uint i = pos; i<VGA_HEIGHT*VGA_WIDTH; i++) {
      VGA_MEMORY[i] = empty;
    }
  }

  // Set cursor position
  outb(VGA_PORT, 14);
  outb(VGA_PORT+1, pos>>8);
  outb(VGA_PORT, 15);
  outb(VGA_PORT+1, pos);
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

  const uint pos = VGA_WIDTH*y + x;
  VGA_MEMORY[pos] = (c & 0xFF) | (attr << 8);

  return;
}

// Clear screen
void io_vga_clear()
{
  const uint16_t empty = ' '|(AT_DEFAULT<<8);
  for(uint x=0; x<VGA_WIDTH; x++) {
    for(uint y=0; y<VGA_HEIGHT; y++) {
      VGA_MEMORY[VGA_WIDTH*y+x] = empty;
    }
  }

  io_vga_setcursorpos(0, 0);
}

// Get screen cursor position
void io_vga_getcursorpos(uint* x, uint* y)
{
  uint pos = 0;
  outb(VGA_PORT, 14);
  pos = inb(VGA_PORT+1) << 8;
  outb(VGA_PORT, 15);
  pos |= inb(VGA_PORT+1);

  *x = pos % VGA_WIDTH;
  *y = pos / VGA_WIDTH;
}

// Set screen cursor position
void io_vga_setcursorpos(uint x, uint y)
{
  uint pos = VGA_WIDTH*y + x;
  outb(VGA_PORT, 14);
  outb(VGA_PORT+1, pos>>8);
  outb(VGA_PORT, 15);
  outb(VGA_PORT+1, pos);
}

// Show or hide screen cursor
void io_vga_showcursor(uint show)
{
  if(!show) {
    outb(VGA_PORT, 0x0A);
    outb(VGA_PORT+1, 0x20);
  } else {
    outb(VGA_PORT, 0x0A);
    outb(VGA_PORT+1, (inb(VGA_PORT+1) & 0xC0) | 0xC);

    outb(VGA_PORT, 0x0B);
    outb(VGA_PORT+1, (inb(VGA_PORT+8) & 0xE0) | 0xE);
  }
}


// Time related functions
// Convert BCD to uint
static uint BCD_to_int(char BCD)
{
    uint h = (BCD >> 4) * 10;
    return h + (BCD & 0xF);
}

// Read CMOS register
static uint8_t read_CMOS(uint8_t reg)
{
  outb(0x70, reg);
  return inb(0x71);
}

#define CMOS_UIP (1 << 7) // RTC update in progress
// Get system time
void io_getdatetime(time_t* t)
{
  // Use the RTC
  while(1) {
    while(read_CMOS(0x0A) & CMOS_UIP);

    t->year   = read_CMOS(0x09);
    if(read_CMOS(0x0A) & CMOS_UIP) continue;
    t->month  = read_CMOS(0x08);
    if(read_CMOS(0x0A) & CMOS_UIP) continue;
    t->day    = read_CMOS(0x07);
    if(read_CMOS(0x0A) & CMOS_UIP) continue;
    t->hour   = read_CMOS(0x04);
    if(read_CMOS(0x0A) & CMOS_UIP) continue;
    t->minute = read_CMOS(0x02);
    if(read_CMOS(0x0A) & CMOS_UIP) continue;
    t->second = read_CMOS(0x00);
    if(read_CMOS(0x0A) & CMOS_UIP) continue;

    break;
  }

  const uint8_t status_b = read_CMOS(0x0B);

  // Convert from BCD if needed
  if(!(status_b & 0x04)) {
    t->year   = BCD_to_int(t->year);
    t->month  = BCD_to_int(t->month);
    t->day    = BCD_to_int(t->day);
    t->hour   = BCD_to_int(t->hour);
    t->minute = BCD_to_int(t->minute);
    t->second = BCD_to_int(t->second);
  }

  // Convert to 24 hour format if needed
  if(!(status_b & 0x02) && (t->hour & 0x80)) {
    t->hour = ((t->hour & 0x7F) + 12) % 24;
  }

  // Compute actual year
  t->year += 2000;
}

// Get current second (raw unknown format)
static uint get_currentsecond()
{
  while(read_CMOS(0x0A) & CMOS_UIP);
  return read_CMOS(0x00);
}


// APIC related
#define IA32_APIC_BASE_MSR 0x1B

// Local APIC registers, divided by 4 for use as uint[] indices
#define ID      (0x0020/4)   // ID
#define VER     (0x0030/4)   // Version
#define TPR     (0x0080/4)   // Task Priority
#define EOI     (0x00B0/4)   // EOI
#define SVR     (0x00F0/4)   // Spurious Interrupt Vector
  #define ENABLE     0x00000100   // Unit Enable
#define ESR     (0x0280/4)   // Error Status
#define ICRLO   (0x0300/4)   // Interrupt Command
  #define INIT       0x00000500   // INIT/RESET
  #define STARTUP    0x00000600   // Startup IPI
  #define DELIVS     0x00001000   // Delivery status
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define DEASSERT   0x00000000
  #define LEVEL      0x00008000   // Level triggered
  #define BCAST      0x00080000   // Send to all APICs, including self.
  #define BUSY       0x00001000
  #define FIXED      0x00000000
#define ICRHI   (0x0310/4)   // Interrupt Command [63:32]
#define TIMER   (0x0320/4)   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // divide counts by 1
  #define PERIODIC   0x00020000   // Periodic
#define PCINT   (0x0340/4)   // Performance Counter LVT
#define LINT0   (0x0350/4)   // Local Vector Table 1 (LINT0)
#define LINT1   (0x0360/4)   // Local Vector Table 2 (LINT1)
#define ERROR   (0x0370/4)   // Local Vector Table 3 (ERROR)
  #define MASKED     0x00010000   // Interrupt masked
#define TICR    (0x0380/4)   // Timer Initial Count
#define TCCR    (0x0390/4)   // Timer Current Count
#define TDCR    (0x03E0/4)   // Timer Divide Configuration

// IRQs
#define T_IRQ0          32      // IRQ 0 corresponds to int T_IRQ

#define IRQ_TIMER        0
#define IRQ_SPURIOUS    31

static volatile uint32_t* lapic;
static void lapicw(uint index, uint value)
{
  lapic[index] = value;
  lapic[ID];  // wait for write to finish, by reading
}

// Miliseconds timer related
static volatile uint clock_ints = 0xFFFFFFFF;
static volatile uint ints_per_second = 5000000;

// Initialize lapic
void lapic_init()
{
  __asm__("cli");

  uint32_t eax, edx;
  read_MSR(IA32_APIC_BASE_MSR, &eax, &edx);

  lapic = (void*)(eax & 0xFFFFF000);
  debug_putstr("LAPIC base=%x\n", lapic);

  // Enable local APIC; set spurious interrupt vector
  lapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  // The timer repeatedly counts down at bus frequency
  // from lapic[TICR] and then issues an interrupt
  lapicw(TDCR, X1);
  lapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  lapicw(TICR, ints_per_second);

  // Clear error status register (requires back-to-back writes)
  lapicw(ESR, 0);
  lapicw(ESR, 0);

  // Ack any outstanding interrupts
  lapicw(EOI, 0);

  // Send an Init Level De-Assert to synchronise arbitration ID's
  lapicw(ICRHI, 0);
  lapicw(ICRLO, INIT | LEVEL);
  while(lapic[ICRLO] & DELIVS);

  // Enable interrupts on the APIC
  lapicw(TPR, 0x0);

  __asm__("sti");
}

// Inhibit LAPIC interrupts
void lapic_inhibit()
{
    lapicw(TPR, 0x20);
}

// Deinhibit LAPIC interrupts
void lapic_deinhibit()
{
    lapicw(TPR, 0x00);
}

// Spurious handler
void spurious_handler()
{
  // Acknowledge
  lapicw(EOI, 0);
}

// Timer handler
void timer_handler()
{
  clock_ints++;

  // Set frequency
  static uint32_t s0 = 0xFFFF;
  static uint32_t s1 = 0xFFFE;
  static uint32_t s2 = 0xFFFD;

  if(s0 == 0xFFFF) {
    s0 = get_currentsecond();
    s1 = s0;
  } else if(s1 == s0) {
    s1 = get_currentsecond();
    if(s1 != s0) {
      s2 = s1;
      clock_ints = 0;
    }
  } else if(s2 == s1) {
    s2 = get_currentsecond();
    if(s2 != s1) {
      lapicw(TICR, (clock_ints * ints_per_second) / 100);
      ints_per_second = 100;
      debug_putstr("Timer adjusted to %u interrupts per second\n", ints_per_second);
    }
  }

  // Acknowledge
  lapicw(EOI, 0);
}

// Get system miliseconds
uint io_gettimer()
{
  return ((clock_ints * 1000) / ints_per_second);
}

// We need this buffer to be inside a 64KB bound
// to avoid DMA error
extern uint8_t disk_buff[DISK_SECTOR_SIZE];
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
  __asm__("stc");
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
  __asm__("clc");
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

#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_DRQ       0x08
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_IDENT 0xEC

// Around two seconds (400ns per port read)
#define ATA_ATTEMPTS  (2e9/400)

// Wait for disk ready. Return 0 on success
static uint ATA_PIO_waitdisk()
{
  uint r = 0;
  for(uint i=0; i<ATA_ATTEMPTS; i++) {
    r = inb(0x1F7);
    if(r & IDE_ERR) {
      debug_putstr("ATA wait disk error: %x\n", r);
      break;
    }
    if((r & (IDE_BSY|IDE_DRDY)) == IDE_DRDY) {
      break;
    }
  }

  return (r & (IDE_DF|IDE_ERR));
}

// Read sectors using ATA PIO mode
// Return 0 on success
static uint ATA_PIO_readsector(uint disk, uint sector, size_t n, void* buff)
{
  // Set disk and wait
  outb(0x1F6, (((disk-2)&1)<<4) | 0xE0);
  uint result = ATA_PIO_waitdisk();
  if(result == 0) {

    // Issue command
    outb(0x1F2, n);
    outb(0x1F3, sector);
    outb(0x1F4, sector >> 8);
    outb(0x1F5, sector >> 16);
    outb(0x1F6, (sector >> 24) | (((disk-2)&1)<<4) | 0xE0);
    outb(0x1F7, IDE_CMD_READ);

    // Read and copy data
    result = ATA_PIO_waitdisk();
    if(result == 0) {
      insl(0x1F0, buff, n*DISK_SECTOR_SIZE/4);
    }
  }
  return result;
}

// Write sectors using ATA PIO mode
// Return 0 on success
static uint ATA_PIO_writesector(uint disk, uint sector, size_t n, const void* buff)
{
  // Set disk and wait
  outb(0x1F6, (((disk-2)&1)<<4) | 0xE0);
  uint result = ATA_PIO_waitdisk();
  if(result != 0) {
    return result;
  }

  // Issue command
  outb(0x1F2, n);
  outb(0x1F3, sector);
  outb(0x1F4, sector >> 8);
  outb(0x1F5, sector >> 16);
  outb(0x1F6, (sector >> 24) | (((disk-2)&1)<<4) | 0xE0);
  outb(0x1F7, IDE_CMD_WRITE);

  // Write and copy data
  outsl(0x1F0, buff, n*DISK_SECTOR_SIZE/4);
  return ATA_PIO_waitdisk();
}

// Detect ATA disk size and model
#define ATADEV_PATAPI 0xEB14
#define ATADEV_SATAPI 0x9669
#define ATADEV_PATA 0x0000
#define ATADEV_SATA 0xC33C
static uint ATA_detect(uint disk, char* model, size_t model_size )
{
  // Send IDENTIFY command
	outb(0x1F6, (((disk-2)&1)<<4) | 0xA0);
  outb(0x1F7, IDE_CMD_IDENT);

  uint result = 0;
  // Check disk exists
  for(uint i=0; i<ATA_ATTEMPTS; i++) {
    result = inb(0x1F7);
    if(result != 0) {
      break;
    }
  }

  // Resturn "no disk"
  if(result == 0) {
    debug_putstr("ATA identifying disk %u: no disk\n", disk);
    return 0;
  }

  // Wait
  for(uint i=0; i<ATA_ATTEMPTS; i++) {
    result = inb(0x1F7);
    if(result & IDE_ERR) {
      debug_putstr("ATA identifying disk %u: error waiting: %x\n",
        disk, result);
      return 0;
    }
    if((result & IDE_BSY) == 0) {
      break;
    }
  }

  // Check ready
  for(uint i=0; i<ATA_ATTEMPTS; i++) {
    result = inb(0x1F7);
    if((result & (IDE_DRQ|IDE_ERR)) != 0) {
      break;
    }
  }

  if((result & IDE_ERR) || !(result & IDE_DRQ)) {
    debug_putstr("ATA identifying disk %u: error not ready: %x\n",
      disk, result);
    return 0;
  }

  // Read identification
  model_size = min(model_size, 40);
  uint16_t identification[2] = {0};
  uint32_t num_sectors = 0;
  for(uint i=0; i<256; i++) {
    identification[i%2] = inw(0x1F0);
    // Byte 54 to 94 contain a model string
    if(i>=27 && i<27+(model_size/2)) {
        model[2*(i-27)+1] = identification[i%2] & 0xFF;
        model[2*(i-27)+0] = (identification[i%2] & 0xFF00)>>8;

    // Words 60 and 61 read as 32-bit contain number of sectors
    } else if(i==61) {
      num_sectors = identification[0] | (identification[1]<<16);
    }
  }

  if(model_size > 0) {
    model[model_size-1] = 0;
  }
  debug_putstr("ATA idenfitying disk %u:_num_sectors: %u\n", disk, num_sectors);
	return num_sectors;
}

// Initialize disks info
void io_disks_init_info()
{
  // Setup disk identifiers
  disk_info[0].id = 0x00; // Floppy disk 0
  strncpy(disk_info[0].name, "fd0", sizeof(disk_info[0].name));
  strncpy(disk_info[0].desc, "", sizeof(disk_info[0].desc));

  disk_info[1].id = 0x01; // Floppy disk 1
  strncpy(disk_info[1].name, "fd1", sizeof(disk_info[1].name));
  strncpy(disk_info[1].desc, "", sizeof(disk_info[1].desc));

  disk_info[2].id = 0x80; // Hard disk 0
  strncpy(disk_info[2].name, "hd0", sizeof(disk_info[2].name));
  strncpy(disk_info[2].desc, "", sizeof(disk_info[2].desc));

  disk_info[3].id = 0x81; // Hard disk 1
  strncpy(disk_info[3].name, "hd1", sizeof(disk_info[3].name));
  strncpy(disk_info[3].desc, "", sizeof(disk_info[3].desc));

  // Initialize hardware related disks info
  for(uint i=0; i<MAX_DISK; i++) {
    uint hwdisk = disk_info[i].id;

    // Try to get info
    diskinfo_t dinfo;
    uint result = disk_get_info(hwdisk, &dinfo);
    disk_info[i].sectors = dinfo.sector_num;
    disk_info[i].sides = dinfo.head_num;
    disk_info[i].cylinders = dinfo.cylinder_num;
    disk_info[i].isATA = 0;

    // Use retrieved data if success
    if(result == 0) {
      disk_info[i].size = (disk_info[i].sectors *
        disk_info[i].sides * disk_info[i].cylinders) /
        (1048576 / DISK_SECTOR_SIZE);

      disk_info[i].last_access = 0;

      if(i >= 2) {
        char model[sizeof(disk_info[i].desc)] = {0};
        uint ATA_num_sectors = ATA_detect(i, model, sizeof(model));
        if(ATA_num_sectors > 0) {
          disk_info[i].isATA = 1;
          disk_info[i].size = (ATA_num_sectors * DISK_SECTOR_SIZE)/(1024*1024);
          if(strlen(model)) {
            strncpy(disk_info[i].desc, model, sizeof(disk_info[i].desc));
          } else {
            strncpy(disk_info[i].desc, "ATA", sizeof(disk_info[i].desc));
          }
        }
      }

      debug_putstr("DISK (%2x : size=%u MB sect_per_track=%d, sides=%d, cylinders=%d) %s\n",
        hwdisk, disk_info[i].size, disk_info[i].sectors, disk_info[i].sides,
        disk_info[i].cylinders, disk_info[i].desc);

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
  uint result = 0;

  // Use ATA PIO for IDE
  if(disk_info[disk].isATA) {
    while(n > 0) {
      uint n_sectors = min(n,255);
      result = ATA_PIO_readsector(disk, sector, n_sectors, buff);
      n -= n_sectors;
      sector += n_sectors;
      buff += n_sectors * DISK_SECTOR_SIZE;
      if(result) {
        break;
      }
    }

  // Use BIOS otherwise
  } else {
    uint hwdisk = disk_to_hwdisk(disk);

    chs_t chs;
    regs16_t regs;

    // Handle DMA access 64kb boundary
    // Write sector by sector using disk buffer
    for(uint s=0; s<n; s++) {
      lba_to_chs(sector+s, disk_info[disk].sectors,
        disk_info[disk].sides, &chs);

      for(uint attempt=0; attempt<3; attempt++) {
        if(attempt>0) {
          result = disk_reset(hwdisk);
          if(result) {
            debug_putstr("io_disk_read_sector: error reseting disk\n");
            break;
          }
        }

        memset(&regs, 0, sizeof(regs));
        regs.ax = (0x02 << 8) | 1;
        regs.cx =
          ((chs.cylinder & 0xFF) << 8) |
          (chs.sector & 0x3F) |
          ((chs.cylinder & 0x300) >> 2);

        regs.dx = (chs.head << 8) | hwdisk;
        regs.es = ((uint)disk_buff) / 0x10000;
        regs.bx = ((uint)disk_buff) % 0x10000;
        __asm__("stc");
        int32(0x13, &regs);

        result = (regs.eflags & X86_CF);
        if(!result) {
          if(buff != disk_buff) {
            memcpy(buff+s*DISK_SECTOR_SIZE, disk_buff, DISK_SECTOR_SIZE);
          }
          break;
        } else {
          result = (regs.ax & 0xFF00) >> 8;
        }
      }

      // Break on error
      if(result) {
        break;
      }
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
    memcpy(buff, disk_buff+offset, i);
    sector++;
    size -= i;
  }

  // Now read aligned an entire sectors
  n_sectors = size / DISK_SECTOR_SIZE;

  if(n_sectors && result == 0) {
    result = disk_read_sector(disk, sector, n_sectors, buff+i);
    i += n_sectors * DISK_SECTOR_SIZE;
    sector += n_sectors;
    size -= n_sectors * DISK_SECTOR_SIZE;
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
  uint result = 0;

  // Use ATA PIO for IDE
  if(disk_info[disk].isATA) {
    while(n > 0) {
      uint n_sectors = min(n,255);
      result = ATA_PIO_writesector(disk, sector, n_sectors, buff);
      n -= n_sectors;
      sector += n_sectors;
      buff += n_sectors * DISK_SECTOR_SIZE;
      if(result) {
        break;
      }
    }

  // Use BIOS otherwise
  } else {
    uint hwdisk = disk_to_hwdisk(disk);

    chs_t chs;
    regs16_t regs;

    // Handle DMA access 64kb boundary
    // Write sector by sector using disk buffer
    for(uint s=0; s<n; s++) {
      if(buff != disk_buff) {
        memcpy(disk_buff, buff+DISK_SECTOR_SIZE*s, DISK_SECTOR_SIZE);
      }

      lba_to_chs(sector+s, disk_info[disk].sectors,
        disk_info[disk].sides, &chs);

      for(uint attempt=0; attempt<3; attempt++) {
        if(attempt>0) {
          result = disk_reset(hwdisk);
          if(result) {
            debug_putstr("io_disk_write_sector: error reseting disk\n");
            break;
          }
        }

        memset(&regs, 0, sizeof(regs));
        regs.ax = (0x03 << 8) | 1;
        regs.cx =
          ((chs.cylinder & 0xFF) << 8) |
          (chs.sector & 0x3F) |
          ((chs.cylinder & 0x300) >> 2);

        regs.dx = (chs.head << 8) | hwdisk;
        regs.es = ((uint)disk_buff) / 0x10000;
        regs.bx = ((uint)disk_buff) % 0x10000;
        __asm__("stc");
        int32(0x13, &regs);

        result = (regs.eflags & X86_CF);
        if(!result) {
          break;
        } else {
          result = (regs.ax & 0xFF00) >> 8;
        }
      }

      // Break on error
      if(result) {
        break;
      }
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
    result = disk_write_sector(disk, sector, n_sectors, buff+i);
    i += n_sectors * DISK_SECTOR_SIZE;
    sector += n_sectors;
    size -= n_sectors * DISK_SECTOR_SIZE;
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

  if(regs.eflags & X86_CF) {
    debug_putstr("APM disconnect error (%2x)\n", (regs.ax & 0xFF00) >> 8);
  }

  // Connect to real mode interface
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x5301;
  regs.bx = 0;
  int32(0x15, &regs);

  if(regs.eflags & X86_CF) {
    debug_putstr("APM connect error (%2x)\n", (regs.ax & 0xFF00) >> 8);
  }

  // Set APM version
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x530E;
  regs.bx = 0;
  regs.cx = 0x0101;
  int32(0x15, &regs);

  if(regs.eflags & X86_CF) {
    debug_putstr("APM set version error (%2x)\n", (regs.ax & 0xFF00) >> 8);
  }

  // Enable power management
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x530D;
  regs.bx = 0x0001;
  regs.cx = 0x0001;
  int32(0x15, &regs);

  if(regs.eflags & X86_CF) {
    debug_putstr("APM enable error (%2x)\n", (regs.ax & 0xFF00) >> 8);
  }

  // Power off
  memset(&regs, 0, sizeof(regs));
  regs.ax = 0x5307;
  regs.bx = 0x0001;
  regs.cx = 0x0003;
  int32(0x15, &regs);

  if(regs.eflags & X86_CF) {
    debug_putstr("APM set state error (%2x)\n", (regs.ax & 0xFF00) >> 8);
  }

  while(1) {

  }
}
