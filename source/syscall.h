// Syscall

#ifndef _SYSCALL_H
#define _SYSCALL_H

// System call service codes
#define SYSCALL_IO_OUT_CHAR             0x0010
#define SYSCALL_IO_GET_CURSOR_POS       0x0018
#define SYSCALL_IO_SET_CURSOR_POS       0x0019
#define SYSCALL_IO_SET_SHOW_CURSOR      0x001A
#define SYSCALL_IO_IN_KEY               0x0020
#define SYSCALL_IO_OUT_CHAR_SERIAL      0x0030
#define SYSCALL_IO_OUT_CHAR_DEBUG       0x0040
#define SYSCALL_FS_GET_INFO             0x0050
#define SYSCALL_FS_GET_ENTRY            0x0051
#define SYSCALL_FS_READ_FILE            0x0052
#define SYSCALL_FS_WRITE_FILE           0x0053
#define SYSCALL_FS_MOVE                 0x0054
#define SYSCALL_FS_COPY                 0x0055
#define SYSCALL_FS_DELETE               0x0056
#define SYSCALL_FS_CREATE_DIRECTORY     0x0057
#define SYSCALL_FS_LIST                 0x0058
#define SYSCALL_FS_FORMAT               0x0059
#define SYSCALL_TIME_GET                0x0060

typedef struct {
  uint x;
  uint y;
} TSYSCALL_POSITION;

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
