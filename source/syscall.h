// Syscall

#ifndef _SYSCALL_H
#define _SYSCALL_H

// System call service codes
#define SYSCALL_MEM_ALLOCATE            0x0000
#define SYSCALL_MEM_FREE                0x0001
#define SYSCALL_IO_OUT_CHAR             0x0020
#define SYSCALL_IO_OUT_CHAR_ATTR        0x0021
#define SYSCALL_IO_CLEAR_SCREEN         0x0022
#define SYSCALL_IO_GET_CURSOR_POS       0x0028
#define SYSCALL_IO_SET_CURSOR_POS       0x0029
#define SYSCALL_IO_SET_SHOW_CURSOR      0x002A
#define SYSCALL_IO_IN_KEY               0x0030
#define SYSCALL_IO_OUT_CHAR_SERIAL      0x0040
#define SYSCALL_IO_OUT_CHAR_DEBUG       0x0050
#define SYSCALL_FS_GET_INFO             0x0060
#define SYSCALL_FS_GET_ENTRY            0x0061
#define SYSCALL_FS_READ_FILE            0x0062
#define SYSCALL_FS_WRITE_FILE           0x0063
#define SYSCALL_FS_MOVE                 0x0064
#define SYSCALL_FS_COPY                 0x0065
#define SYSCALL_FS_DELETE               0x0066
#define SYSCALL_FS_CREATE_DIRECTORY     0x0067
#define SYSCALL_FS_LIST                 0x0068
#define SYSCALL_FS_FORMAT               0x0069
#define SYSCALL_DATETIME_GET            0x0070
#define SYSCALL_TIMER_GET               0x0071

typedef struct {
  uint x;
  uint y;
} TSYSCALL_POSITION;

typedef struct {
  uint x;
  uint y;
  char c;
  uint attr;
} TSYSCALL_POSATTR;

typedef struct {
  uint               disk_index;
  FS_INFO*           info;
} TSYSCALL_FSINFO;

typedef struct {
  FS_ENTRY*          entry;
  char*              path;
  uint               parent;
  uint               disk;
} TSYSCALL_FSENTRY;

typedef struct {
  void*              buff;
  char*              path;
  uint               offset;
  uint               count;
  uint               flags;
} TSYSCALL_FSRWFILE;

typedef struct {
  char*              src;
  char*              dst;
} TSYSCALL_FSSRCDST;

typedef struct {
  FS_ENTRY*          entry;
  char*              path;
  uint               n;
} TSYSCALL_FSLIST;

#endif // _SYSCALL_H
