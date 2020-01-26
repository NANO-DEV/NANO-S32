// User library

#ifndef _ULIB_H
#define _ULIB_H

#define min(a,b) (a<b?a:b)
#define max(a,b) (a>b?a:b)

// System call
// Avoid using it since there are already implemented
// more general purpose functions in this library
uint syscall(uint service, void *param);

// Memory
void  *memset(void *dst, int c, size_t n);
size_t memcpy(void *dst, const void *src, size_t n);
size_t memcmp(const void *mem1, const void *mem2, size_t n);

// String management
size_t strcmp(const char *str1, const char *str2);
size_t strlen(const char *str);
size_t strncpy(char *dst, const char *src, size_t n);
size_t strncat(char *dst, const char *src, size_t n);
size_t strchr(const char *str, char c);
char  *strtok(char *src, char **next, char delim);

// strtok:
// Get first token in string
//
// src is the input untokenized string. It will be modified.
// delim is the delimiter character to separate tokens.
// The function returns a pointer to the start of first token in src.
// All consecutive delimiters found immediately after the first token
// will be replaced with 0, so return value is a 0 finished string with
// the first token.
// After execution, (*next) will point to the remaining string after the
// first token and its final 0 (or 0s).
//
// To tokenize a full string, this function should be called several times,
// until *next = 0

// Parse string uint
uint stou(char *src);

// Format a string using %x, %d, %u, %U, %s...
void formatstr(char *str, size_t size, char *format, ...);


// Allocate size bytes of memory
void *malloc(size_t size);

// Free allocated memory
void mfree(void *ptr);


// Cursor management
void get_cursor_pos(uint *col, uint *row);
void set_cursor_pos(uint col, uint row);
void set_show_cursor(bool show);


// Special key codes
//
// getkey() function returns a key code (uint)
// Alphanumeric and usual symbol key codes are their ASCII code.
// Special key values are specified here.
// Example:
//
// uint k = getkey();
// if(k == KEY_DEL) ...
// if(k == 'a') ...

#define KEY_BACKSPACE 0x08
#define KEY_RETURN    0x0D
#define KEY_ESC       0x1B
#define KEY_DEL       0xE0
#define KEY_END       0xE1
#define KEY_HOME      0xE2
#define KEY_INS       0xE3
#define KEY_PG_DN     0xE4
#define KEY_PG_UP     0xE5
#define KEY_PRT_SC    0xE6
#define KEY_TAB       0x09

#define KEY_UP        0xE7
#define KEY_LEFT      0xE8
#define KEY_RIGHT     0xE9
#define KEY_DOWN      0xEA

#define KEY_F1        0xF1
#define KEY_F2        0xF2
#define KEY_F3        0xF3
#define KEY_F4        0xF4
#define KEY_F5        0xF5
#define KEY_F6        0xF6
#define KEY_F7        0xF7
#define KEY_F8        0xF8
#define KEY_F9        0xF9
#define KEY_F10       0xFA
#define KEY_F11       0xFB
#define KEY_F12       0xFC

enum GETKEY_WAITMODE{
  GETKEY_WAITMODE_NOWAIT = 0, // Return 0 if no key pressed
  GETKEY_WAITMODE_WAIT        // Wait for key pressed
};
uint getkey(uint wait_mode); // See GETKEY_WAITMODE enum
int getstr(char *str, size_t n);
// getstr: Get a string from user. Returns when RETURN key is
// pressed. Unused str characters are set to 0.
// Returns number of elements in str

// Attribute flags for function putc_attr
// AT_T_ defines text color
// AT_B_ defines background color
#define AT_T_BLACK    0x00
#define AT_T_BLUE     0x01
#define AT_T_GREEN    0x02
#define AT_T_CYAN     0x03
#define AT_T_RED      0x04
#define AT_T_MAGENTA  0x05
#define AT_T_BROWN    0x06
#define AT_T_LGRAY    0x07
#define AT_T_DGRAY    0x08
#define AT_T_LBLUE    0x09
#define AT_T_LGREEN   0x0A
#define AT_T_LCYAN    0x0B
#define AT_T_LRED     0x0C
#define AT_T_LMAGENTA 0x0D
#define AT_T_YELLOW   0x0E
#define AT_T_WHITE    0x0F

#define AT_B_BLACK    0x00
#define AT_B_BLUE     0x10
#define AT_B_GREEN    0x20
#define AT_B_CYAN     0x30
#define AT_B_RED      0x40
#define AT_B_MAGENTA  0x50
#define AT_B_BROWN    0x60
#define AT_B_LGRAY    0x70
#define AT_B_DGRAY    0x80
#define AT_B_LBLUE    0x90
#define AT_B_LGREEN   0xA0
#define AT_B_LCYAN    0xB0
#define AT_B_LRED     0xC0
#define AT_B_LMAGENTA 0xD0
#define AT_B_YELLOW   0xE0
#define AT_B_WHITE    0xF0

// Put char and put formatted string in screen
void putc(char c);
void putc_attr(uint col, uint row, char c, uint8_t attr);
void clear_screen();

// Put char and put formatted string in screen
// Supports:
// %d (int), %u (uint), %x (uint),
// %s (char*), %c (uchar)
// Width modifiers allowed: %2d, %4x...
void putstr(const char *format, ...);

// Formatted strings in serial port and debug output.
// Debug output is serial port by default
void serial_putstr(const char *format, ...);
void debug_putstr(const char *format, ...);



// Get current system date and time
void get_datetime(time_t *t);

// Get current system timer (miliseconds)
uint get_timer();

// Wait a number of miliseconds
void wait(uint ms);


// File system related

// Unless another thing is specified, all paths must be provided as
// absolute or relative to the system disk.
// When specified as absolute, they must begin with a disk identifier.
// Possible disk identifiers are:
// fd0 - First floppy disk
// fd1 - Second floppy disk
// hd0 - First hard disk
// hd1 - Second hard disk
//
// Path components are separated with slashes '/'
// The root directory of a disk can be omitted or referred as "."
// when it's the last path component.
//
// Examples of valid paths:
// fd0
// hd0/.
// hd0/documents/file.txt
//

// When disks must be referenced by uint disk, valid values are:
// fd0 : 0x00
// fd1 : 0x01
// hd0 : 0x80
// hd1 : 0x81

// Special error codes
#define ERROR_NOT_FOUND     0xFFFFFFFF
#define ERROR_EXISTS        0xFFFFFFFE
#define ERROR_IO            0xFFFFFFFD
#define ERROR_NO_SPACE      0xFFFFFFFC
#define ERROR_NOT_AVAILABLE 0xFFFFFFFB
#define ERROR_ANY           0xFFFFFFFA
#define NO_ERROR            0x00000000

// fs_entry_t flags
#define FST_DIR  0x01   // Directory
#define FST_FILE 0x02   // File

typedef struct fs_entry_t {
  char    name[15];
  uint8_t flags;
  uint    size; // bytes for files, items for directories
} fs_entry_t;

#define MAX_PATH 72

// fs_info_t.fs_type types
#define FS_TYPE_UNKNOWN 0x000
#define FS_TYPE_NSFS    0x001

typedef struct fs_info_t{
  char  name[4];
  uint  id;        // Disk id code
  uint  fs_type;
  uint  fs_size;   // MB
  uint  disk_size; // MB
} fs_info_t;


// Get filesystem info
// Output: info
// disk_index is referred to the index of a disk on the currently
// available disks list.
// returns number of available disks
uint get_fsinfo(uint disk_index, fs_info_t *info);


// Get filesystem entry
// Output: entry
// parent and/or disk can be UNKNOWN_VALUE if they are unknown.
// In this case they will be deducted from path
// Paths must be:
// - absolute or relative to system disk if parent and/or disk are unknown
// - relative to parent if parent index entry or disk id are provided
// Returns ERROR_NOT_FOUND if error, entry index otherwise
#define UNKNOWN_VALUE 0xFFFFFFFF
uint get_entry(fs_entry_t *entry, char *path, uint parent, uint disk);


// Read file
// Output: buff
// Reads count bytes of path file starting at byte offset inside this file.
// Returns number of readed bytes or ERROR_NOT_FOUND
uint read_file(void *buff, char *path, uint offset, uint count);

// Write file flags
 #define FWF_CREATE   0x0001 // Create if does not exist
 #define FWF_TRUNCATE 0x0002 // Truncate to last written position

// Write file
// Writes count bytes of path file starting at byte offset inside this file.
// If target file is not big enough, its size is increased.
// Depending on flags, path file can be created or truncated.
// Returns number of written bytes or ERROR_NOT_FOUND
uint write_file(void *buff, char *path, uint offset, uint count, uint flags);


// Move entry
// In the case of directories, they are recursively moved
// Returns:
// - ERROR_NOT_FOUND if source does not exist
// - ERROR_EXISTS if destination exists
// - another value otherwise
uint move(char *srcpath, char *dstpath);


// Copy entry
// In the case of directories, they are recursively copied
// Returns:
// - ERROR_NOT_FOUND if source does not exist
// - ERROR_EXISTS if destination exists
// - another value otherwise
uint copy(char *srcpath, char *dstpath);

// Delete entry
// In the case of directories, they are recursively deleted
// Returns:
// - ERROR_NOT_FOUND if path does not exist
// - index of deleted entry otherwise
uint delete(char *path);


// Create a directory
// Returns:
// - ERROR_NOT_FOUND if parent path does not exist
// - ERROR_EXISTS if destination already exists
// - index of created entry otherwise
uint create_directory(char *path);


// List directory entries
// Output: entry
// Gets entry with index n in path directory
// Returns:
// - ERROR_NOT_FOUND if path does not exist
// - number of elements in ths directory otherwise
uint list(fs_entry_t *entry, char *path, uint n);


// Create filesystem in disk
// Deletes all files, creates NSFS filesystem
// and adds a copy of the kernel.
// Returns 0 on success
uint format(uint disk);



// About IP format
// Unless something different is specified:
// uint8_t *ip  are arrays of 4 bytes with a parsed IP
// char *ip     are strings containing an unparsed IP
#define IP_LEN 4
typedef struct net_address_t {
  uint8_t  ip[IP_LEN];
  uint16_t port;
} net_address_t;

// Convert string to IP
void str_to_ip(uint8_t *ip, const char *str);

// Convert IP to string
char *ip_to_str(char *str, uint8_t *ip);

// Send len bytes of buff to dst through network
// Uses UDP protocol. Reception not guaranteed
// (uses UDP port 8086 for source)
// Returns NO_ERROR on success
uint send(net_address_t *dst, uint8_t *buff, size_t len);

// Get and remove received data from the network reception buffer.
// src and buff are filled by the function.
// If more than buff_size bytes were received,
// these will be lost.
// Returns number of bytes of buff that have been filled
// or 0 if nothing received.
// When the reception buffer is full, all new received packets
// are discarded. So, recv must be called frequently
// Call recv_set_port once before start calling to this
// function to enable a reception port.
// Uses UDP protocol
uint recv(net_address_t *src, uint8_t *buff, size_t buff_size);

// Enable reception in this port and disable all others.
// Must be called once before start calling recv.
// Specify a UDP port as parameter
void recv_set_port(uint16_t port);



// Play sound file, non blocking
// Return NO_ERROR on success, error code otherwise.
// Can only play wav files
uint sound_play(const char *wav_file);

// Return true while a sound is playing
// Return false otherwise
bool sound_is_playing();

// Stop playing sound
void sound_stop();


#endif // _ULIB_H
