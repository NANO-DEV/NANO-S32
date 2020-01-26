// User library

#include "types.h"
#include "ulib.h"
#include "syscall.h"

// System call
uint syscall(uint service, void *param)
{
  uint __res = 0;
  __asm__ volatile( "int $49;"
                  : "=a" ((uint)(__res))
                  : "0" (service), "b" ((uint)(service)),"c" ((void*)(param))
                  : "memory"
                );
  return __res;
}

// x86 specific helper function
static inline void stosb(void *addr, int data, size_t cnt)
{
  __asm__ volatile("cld; rep stosb" :
               "=D" (addr), "=c" (cnt) :
               "0" (addr), "1" (cnt), "a" (data) :
               "memory", "cc");
}

// Compare strings
size_t strcmp(const char *str1, const char *str2)
{
  size_t i = 0;
  while(str1[i]==str2[i] && str1[i]!=0) {
    i++;
  }
  return str1[i] - str2[i];
}

// Get string length
size_t strlen(const char *str)
{
  size_t len = 0;
  while(str[len]) {
    len++;
  }
  return len;
}

// Copy string src to dst without exceeding
// n elements in dst
size_t strncpy(char *dst, const char *src, size_t n)
{
  size_t i = 0;
  while(src[i]!=0 && i+1<n) {
    dst[i] = src[i];
    i++;
  }
  dst[i] = 0;
  return i;
}

// Concatenate string src to dst, without exceeding
// n elements in dst
size_t strncat(char *dst, const char *src, size_t n)
{
  size_t j = 0;
  size_t i = strlen(dst);
  while(src[j]!=0 && i+1<n) {
    dst[i] = src[j];
    i++;
    j++;
  }
  dst[i] = 0;
  return i;
}

// Tokenize string
char *strtok(char *src, char **next, char delim)
{
  while(*src == delim) {
    *src = 0;
    src++;
  }

  char *s = src;

  while(*s) {
    if(*s == delim) {
      *s = 0;
      *next = s+1;
      return src;
    }
    s++;
  }

  *next = s;
  return src;
}

// Find char in string
size_t strchr(const char *str, char c)
{
  size_t n = 0;
  while(str[n]) {
    if(str[n] == c) {
      return n+1;
    }
    n++;
  }
  return 0;
}

// Parse string uint
uint stou(char *src)
{
  uint base = 10;
  uint value = 0;

  // Is hex?
  if(src[0]=='0' && src[1]=='x') {
    src += 2;
    base = 16;
  }

  while(*src) {
    uint digit = *src<='9'? *src-'0' :
      *src>='a'? 0xA+*src-'a' : 0xA+*src-'A';

    value = value*base + digit;
    src++;
  }
  return value;
}

 // Set n bytes from dst to c
void *memset(void *dst, int c, size_t n)
{
  stosb(dst, c, n);
  return dst;
}

// Copy n bytes from src to dst
size_t memcpy(void *dst, const void *src, uint n)
{
  uint8_t *vdst = dst;
  const uint8_t *vsrc = src;
  const uint rdir = (src>dst)?0:1;

  size_t i = 0;
  for(i=0; i<n; i++) {
    const size_t c = rdir?n-(i+1):i;
    vdst[c] = vsrc[c];
  }
  return i;
}

// Compare n bytes from mem1 and mem2
size_t memcmp(const void *mem1, const void *mem2, size_t n)
{
  const uint8_t *vmem1 = mem1;
  const uint8_t *vmem2 = mem2;

  for(size_t i=0; i<n; i++) {
    if(vmem1[i] != vmem2[i]) {
      return vmem1[i]-vmem2[i];
    }
  }
  return 0;
}

// Allocate memory
void *malloc(size_t size)
{
  return (void*)syscall(SYSCALL_MEM_ALLOCATE, &size);
}

// Free memory
void mfree(void *ptr)
{
  syscall(SYSCALL_MEM_FREE, ptr);
}

// Clear the screen
void clear_screen()
{
  syscall(SYSCALL_IO_CLEAR_SCREEN, 0);
}

// Format and output a string char by char
// calling a putcharf_t for each output char
// Supports: %d (int), %u (uint), %x (uint),
// %s (char*), %c (char)
#define D_STR_SIZE 24
typedef void (putcharf_t)(char);
static void formatstr_putf(const char *format, uint *args, putcharf_t putchar)
{
  size_t char_count = 0;

  while(*format) {
    if(*format == '%') {
      uint value = *(uint*)(args++);
      uint is_negative = (value & 0x80000000);
      uint base = 0;
      uint n_digits = 0;

      format++;

      while(*format>='0' && *format<='9') {
        n_digits=n_digits*10 + *format-'0';
        format++;
      }

      if(*format == 'd') {
        base = 10;
        if(is_negative) {
          value = -(int)value;
        }
      } else if(*format == 'u') {
        base = 10;
      } else if(*format == 'x') {
        base = 16;
        n_digits = n_digits?n_digits:8;
      } else if(*format == 's') {
        while(*(char*)value) {
          putchar(*(char*)(value++));
          char_count++;
        }
        format++;
      } else if(*format == 'c') {
        if(n_digits) {
          while(char_count<n_digits) {
            putchar((char)value);
            char_count++;
          }
        } else {
          putchar((char)value);
          char_count++;
        }
        format++;
      }

      if(base != 0) {

        if(*format == 'x') {
          putchar('0');
          char_count++;
          putchar('x');
          char_count++;
        } else if(*format == 'd' && is_negative) {
          putchar('-');
          char_count++;
        }

        size_t n = 0;
        char digit[D_STR_SIZE];
        digit[D_STR_SIZE-1] = 0;

        do {
          uint d = (value % base);
          n++;
          digit[D_STR_SIZE-1-n] = (d<=9 ? '0'+d : 'A'+d-10);
          value = value / base;
        } while((value && n < D_STR_SIZE-1) || (n < n_digits));

        value = (uint)&(digit[D_STR_SIZE-1-n]);
        while(*(char*)value) {
          putchar(*(char*)(value++));
          char_count++;
        }
        format++;
      }
    } else {
      putchar(*(format++));
      char_count++;
    }
  }
}

// Format a string (auxiliar elements)
static char *ststr;
static size_t ststr_size;
static void stcatchar(char c)
{
  ststr[min(strlen(ststr), ststr_size-1)] = c;
}
// Format a string
void formatstr(char *str, size_t size, char *format, ...)
{
  ststr = str;
  ststr_size = size;
  memset(ststr, 0, size);
  formatstr_putf(format, (uint*)&format+1,
    *(putcharf_t*)&stcatchar);

  ststr[min(strlen(ststr), size-1)] = 0;
}

// Put char serial port
static void serial_puc(char c)
{
  syscall(SYSCALL_IO_OUT_CHAR_SERIAL, &c);
}

// Put formatted string serial port
void serial_putstr(const char *format, ...)
{
  formatstr_putf(format, (uint*)&format+1,
    *(putcharf_t*)&serial_puc);
  return;
}

// Put char debug output
static void debug_puc(char c)
{
  syscall(SYSCALL_IO_OUT_CHAR_DEBUG, &c);
}

// Put formatted string debug output
void debug_putstr(const char *format, ...)
{
  formatstr_putf(format, (uint*)&format+1,
    *(putcharf_t*)&debug_puc);
  return;
}

// Put char in screen
void putc(char c)
{
  uint ac = (uint8_t)c;
  syscall(SYSCALL_IO_OUT_CHAR, &ac);
}

// Put char in screen
void putc_attr(uint col, uint row, char c, uint8_t attr)
{
  syscall_posattr_t ca = {0};
  ca.x = col;
  ca.y = row;
  ca.c = c;
  ca.attr = attr;
  syscall(SYSCALL_IO_OUT_CHAR_ATTR, &ca);
}

// Put formatted string in screen
void putstr(const char *format, ...)
{
  formatstr_putf(format, (uint*)&format+1,
    *(putcharf_t*)&putc);
  return;
}

// Get cursor position
void get_cursor_pos(uint *col, uint *row)
{
  syscall_porition_t p = {0};
  syscall(SYSCALL_IO_GET_CURSOR_POS, &p);
  *col = p.x;
  *row = p.y;
}

// Set cursor position
void set_cursor_pos(uint col, uint row)
{
  syscall_porition_t p = {0};
  p.x = col;
  p.y = row;
  syscall(SYSCALL_IO_SET_CURSOR_POS, &p);
}

// Show or hide cursor
void set_show_cursor(bool show)
{
  syscall(SYSCALL_IO_SET_SHOW_CURSOR, &show);
}

// Get pressed key
uint getkey(uint mode)
{
  return syscall(SYSCALL_IO_IN_KEY, &mode);
}

// Get a string from user. Returns when RETURN key is
// pressed. Unused str characters are set to 0.
// Returns number of elements in str
int getstr(char *str, size_t n)
{
  uint col=0, row=0;
  uint i = 0;
  uint k = 0;

  // Reset string
  memset(str, 0, n);

  // Get cursor position and show it
  get_cursor_pos(&col, &row);
  set_show_cursor(1);

  do {
    // Get a key press
    k = getkey(GETKEY_WAITMODE_WAIT);

    if(k==0){
      continue;
    }

    // Backspace
    if(k == KEY_BACKSPACE) {
      if(i > 0) {
        memcpy(&str[i-1], &str[i], n-i);
        i--;
      }

    // Delete
    } else if(k == KEY_DEL) {
      if(i < n-1) {
        memcpy(&str[i], &str[i+1], n-i-1);
      }

    // Return
    } else if(k == KEY_RETURN) {
      break;

    // Cursor movement keys
    } else if(k == KEY_LEFT && i>0) {
      i--;
    } else if(k == KEY_RIGHT && i<strlen(str)) {
      i++;
    } else if(k == KEY_HOME) {
      i = 0;
    } else if(k == KEY_END) {
      i = strlen(str);

    // Not useful or allowed char
    } else if(k!=KEY_TAB && (k<0x20 || k>0x7E)) {
      continue;

    // Append char to string
    } else if(strlen(str) < n-1 && (k & 0xFF)) {
        memcpy(&str[i+1], &str[i], n-i-2);
        str[i++] = (k & 0xFF);
    }

    // Hide cursor and redraw string at initial position
    set_show_cursor(0);
    set_cursor_pos(col, row);
    for(uint p=0; p<n; p++) {
      putc(str[p]);
    }

    // Reset cursor
    set_cursor_pos(col+i, row);
    set_show_cursor(1);
  } while(k != KEY_RETURN);

  // Done. Print new line
  str[n-1] = 0;
  putc('\n');

  return strlen(str);
}

// Get filesystem info
uint get_fsinfo(uint disk_index, fs_info_t *info)
{
  syscall_fsinfo_t fi = {0};
  fi.disk_index = disk_index;
  fi.info = info;
  return syscall(SYSCALL_FS_GET_INFO, &fi);
}

// Get filesystem entry
uint get_entry(fs_entry_t *entry, char *path, uint parent, uint disk)
{
  syscall_fsentry_t fi = {0};
  fi.entry = entry;
  fi.path = path;
  fi.parent = parent;
  fi.disk = disk;
  return syscall(SYSCALL_FS_GET_ENTRY, &fi);
}

// Read file
uint read_file(void *buff, char *path, uint offset, uint count)
{
  syscall_fsrwfile_t fi = {0};
  fi.buff = buff;
  fi.path = path;
  fi.offset = offset;
  fi.count = count;
  fi.flags = 0;
  return syscall(SYSCALL_FS_READ_FILE, &fi);
}

// Write file
uint write_file(void *buff, char *path, uint offset, uint count, uint flags)
{
  syscall_fsrwfile_t fi = {0};
  fi.buff = buff;
  fi.path = path;
  fi.offset = offset;
  fi.count = count;
  fi.flags = flags;
  return syscall(SYSCALL_FS_WRITE_FILE, &fi);
}

// Move entry
uint move(char *srcpath, char *dstpath)
{
  syscall_fssrcdst_t fi = {0};
  fi.src = srcpath;
  fi.dst = dstpath;
  return syscall(SYSCALL_FS_MOVE, &fi);
}

// Copy entry
uint copy(char *srcpath, char *dstpath)
{
  syscall_fssrcdst_t fi = {0};
  fi.src = srcpath;
  fi.dst = dstpath;
  return syscall(SYSCALL_FS_COPY, &fi);
}

// Delete entry
uint delete(char *path)
{
  return syscall(SYSCALL_FS_DELETE, path);
}

// Create a directory
uint create_directory(char *path)
{
  return syscall(SYSCALL_FS_CREATE_DIRECTORY, path);
}

// List dir entries
uint list(fs_entry_t *entry, char *path, uint n)
{
  syscall_fslist_t fi = {0};
  fi.entry = entry;
  fi.path = path;
  fi.n = n;
  return syscall(SYSCALL_FS_LIST, &fi);
}

// Create filesystem in disk
uint format(uint disk)
{
  return syscall(SYSCALL_FS_FORMAT, &disk);
}

// Get current system date and time
void get_datetime(time_t *t)
{
  syscall(SYSCALL_DATETIME_GET, t);
}

// Get current system timer (miliseconds)
uint get_timer()
{
  return syscall(SYSCALL_TIMER_GET, 0);
}

// Wait a number of miliseconds
void wait(uint ms)
{
  const uint start_time = get_timer();
  const uint finish_time = start_time+ms;
  while(get_timer() < finish_time) {
    __asm__ volatile("nop");
  }
}

// Convert string to IP
void str_to_ip(uint8_t *ip, const char *str)
{
  uint i = 0;
  char tok_str[24] = {0};
  char *tok = tok_str;
  char *nexttok = tok;
  strncpy(tok_str, str, sizeof(tok_str));

  // Tokenize
  while(*tok && *nexttok && i<4) {
    tok = strtok(tok, &nexttok, '.');
    if(*tok) {
      ip[i++] = stou(tok);
   }
   tok = nexttok;
  }
}

// Convert IP to string
char *ip_to_str(char *str, uint8_t *ip)
{
  formatstr(str, 16, "%u.%u.%u.%u",
    ip[0], ip[1], ip[2], ip[3]);
  return str;
}

// Get and remove from buffer received data.
// src and buff are filled by the function
uint recv(net_address_t *src, uint8_t *buff, size_t buff_size)
{
  syscall_netop_t no = {0};
  no.addr = src;
  no.buff = buff;
  no.size = buff_size;
  return syscall(SYSCALL_NET_RECV, &no);
}

// Send buffer to dst
uint send(net_address_t *dst, uint8_t *buff, size_t len)
{
  syscall_netop_t no = {0};
  no.addr = dst;
  no.buff = buff;
  no.size = len;
  return syscall(SYSCALL_NET_SEND, &no);
}

// Set reception to this port
// Must be set once before calling recv
void recv_set_port(uint16_t port)
{
  syscall(SYSCALL_NET_PORT, &port);
}

// Play sound file, non blocking
uint sound_play(const char *wav_file)
{
  return syscall(SYSCALL_SOUND_PLAY, (void*)wav_file);
}

// Return 1 if a sound is playing
// Return 0 otherwise
bool sound_is_playing()
{
  return syscall(SYSCALL_SOUND_IS_PLAYING, NULL);
}

// Stop playing sound
void sound_stop()
{
  syscall(SYSCALL_SOUND_STOP, NULL);
}
