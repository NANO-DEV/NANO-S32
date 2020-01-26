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
#define SYSCALL_NET_RECV                0x0080
#define SYSCALL_NET_SEND                0x0081
#define SYSCALL_NET_PORT                0x0082
#define SYSCALL_SOUND_PLAY              0x0090
#define SYSCALL_SOUND_STOP              0x0091
#define SYSCALL_SOUND_IS_PLAYING        0x0092

typedef struct syscall_porition_t {
  uint x;
  uint y;
} syscall_porition_t;

typedef struct syscall_posattr_t {
  uint x;
  uint y;
  char c;
  uint attr;
} syscall_posattr_t;

typedef struct syscall_fsinfo_t {
  uint       disk_index;
  fs_info_t *info;
} syscall_fsinfo_t;

typedef struct syscall_fsentry_t {
  fs_entry_t *entry;
  char       *path;
  uint        parent;
  uint        disk;
} syscall_fsentry_t;

typedef struct syscall_fsrwfile_t {
  void *buff;
  char *path;
  uint  offset;
  uint  count;
  uint  flags;
} syscall_fsrwfile_t;

typedef struct syscall_fssrcdst_t {
  char *src;
  char *dst;
} syscall_fssrcdst_t;

typedef struct syscall_fslist_t {
  fs_entry_t *entry;
  char       *path;
  uint        n;
} syscall_fslist_t;

typedef struct syscall_netop_t {
  net_address_t *addr;
  uint8_t       *buff;
  size_t         size;
} syscall_netop_t;

#endif // _SYSCALL_H
