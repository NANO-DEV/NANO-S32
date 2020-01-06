// Kernel

#include "types.h"
#include "kernel.h"
#include "hwio.h"
#include "ulib/ulib.h"
#include "fs.h"
#include "syscall.h"
#include "x86.h"
#include "cli.h"

uint8_t system_disk = 0xFF;

// Heap related
static const void* HEAPADDR = (void*)0x30000; // Allocate heap memory here
#define HEAP_NUM_BLOCK   0x00020U
#define HEAP_MEM_SIZE    0x40000U
#define HEAP_BLOCK_SIZE  (HEAP_MEM_SIZE / HEAP_NUM_BLOCK)

typedef struct {
  uint   used;
  void*  ptr;
} HEAPBLOCK;
static HEAPBLOCK heap[HEAP_NUM_BLOCK];

// Init heap: all blocks are unused
static void heap_init()
{
  uint i;
  for(i=0; i<HEAP_NUM_BLOCK; i++) {
    heap[i].used = 0;
    heap[i].ptr = 0;
  }
}

// Allocate memory in heap
static void* heap_alloc(size_t size)
{
  uint n_alloc = 0;
  uint n_found = 0;
  uint i, j;

  if(size == 0) {
    return 0;
  }

  // Get number of blocks to allocate
  n_alloc = size / HEAP_BLOCK_SIZE +
    ((size % HEAP_BLOCK_SIZE) ? 1 : 0);

  debug_putstr("heap: looking for %u blocks\n", n_alloc);

  // Find a continuous set of n_alloc free blocks
  for(i=0; i<HEAP_NUM_BLOCK; i++) {
    if(heap[i].used) {
      debug_putstr("heap: block %u is in use (%u)\n", i, heap[i].used);
      n_found = 0;
    } else {
      n_found++;
      if(n_found >= n_alloc) {
        uint bi = (i+1-n_alloc)*HEAP_BLOCK_SIZE;
        void* addr = (void*)HEAPADDR + bi;
        for(j=i+1-n_alloc; j<=i; j++) {
          heap[j].ptr = addr;
          heap[j].used = 1;
        }
        debug_putstr("heap: found at %x\n", addr);
        return addr;
      }
    }
  }

  // Error: not found
  debug_putstr("Mem alloc: BAD ALLOC (%u bytes)\n", size);
  return 0;
}

// Free memory in heap
static void heap_free(const void* ptr)
{
  uint i;
  if(ptr != 0) {
    for(i=0; i<HEAP_NUM_BLOCK; i++) {
      if(heap[i].ptr == ptr && heap[i].used) {
        heap[i].used = 0;
        heap[i].ptr = 0;
      }
    }
  }

  return;
}


// Handle system calls
// Usually:
// -Unpack parameters
// -Redirect to the right kernel function
// -Pack and return parameters
uint kernel_service(uint service, void* param)
{
  switch(service) {

    case SYSCALL_MEM_ALLOCATE: {
      return (uint)heap_alloc(*(size_t*)param);
    }

    case SYSCALL_MEM_FREE: {
      heap_free(param);
      return 0;
    }

    case SYSCALL_IO_OUT_CHAR: {
      uint uparam = *(uint*)param;
      uint8_t attr = ((uparam & 0xFF00) >> 8);
      char c = (uparam & 0xFF);
      io_vga_putc(c, attr);
      return 0;
    }

    case SYSCALL_IO_OUT_CHAR_ATTR: {
      TSYSCALL_POSATTR* pc = param;
      io_vga_putc_attr(pc->x, pc->y, pc->c, pc->attr);
      return 0;
    }

    case SYSCALL_IO_CLEAR_SCREEN: {
      io_vga_clear();
      return 0;
    }

    case SYSCALL_IO_SET_CURSOR_POS: {
      TSYSCALL_POSITION* ps = param;
      io_vga_setcursorpos(ps->x, ps->y);
      return 0;
    }

    case SYSCALL_IO_GET_CURSOR_POS: {
      TSYSCALL_POSITION* ps = param;
      io_vga_getcursorpos(&ps->x, &ps->y);
      return 0;
    }

    case SYSCALL_IO_SET_SHOW_CURSOR: {
      io_vga_showcursor(*(uint*)param);
      return 0;
    }

    case SYSCALL_IO_IN_KEY: {
      return io_getkey();
    }

    case SYSCALL_IO_OUT_CHAR_SERIAL:
    case SYSCALL_IO_OUT_CHAR_DEBUG: {
      io_serial_putc(*(char*)param);
      return 0;
    }

    case SYSCALL_FS_GET_INFO: {
      TSYSCALL_FSINFO* fi = param;
      return fs_get_info(fi->disk_index, fi->info);
    }

    case SYSCALL_FS_GET_ENTRY: {
      TSYSCALL_FSENTRY* fi = param;
      SFS_ENTRY entry;
      FS_ENTRY o_entry;
      char path[MAX_PATH];
      uint result;
      memcpy(path, fi->path, sizeof(path));
      result = fs_get_entry(&entry, path, fi->parent, fi->disk);
      memcpy(o_entry.name, entry.name, sizeof(o_entry.name));
      o_entry.flags = entry.flags;
      o_entry.size = entry.size;
      memcpy(fi->entry, &o_entry, sizeof(o_entry));
      return result;
    }

    case SYSCALL_FS_READ_FILE: {
      TSYSCALL_FSRWFILE* fi = param;
      char path[MAX_PATH];
      memcpy(path, fi->path, sizeof(path));
      return fs_read_file(fi->buff, path, fi->offset, fi->count);
    }

    case SYSCALL_FS_WRITE_FILE: {
      TSYSCALL_FSRWFILE* fi = param;
      char path[MAX_PATH];
      memcpy(path, fi->path, sizeof(path));
      return fs_write_file(fi->buff, path, fi->offset, fi->count, fi->flags);
    }

    case SYSCALL_FS_MOVE: {
      TSYSCALL_FSSRCDST* fi = param;
      char src[MAX_PATH];
      char dst[MAX_PATH];
      memcpy(src, fi->src, sizeof(src));
      memcpy(dst, fi->dst, sizeof(dst));
      return fs_move(src, dst);
    }

    case SYSCALL_FS_COPY: {
      TSYSCALL_FSSRCDST* fi = param;
      char src[MAX_PATH];
      char dst[MAX_PATH];
      memcpy(src, fi->src, sizeof(src));
      memcpy(dst, fi->dst, sizeof(dst));
      return fs_copy(src, dst);
    }

    case SYSCALL_FS_DELETE: {
      char path[MAX_PATH];
      memcpy(path, param, sizeof(path));
      return fs_delete(path);
    }

    case SYSCALL_FS_CREATE_DIRECTORY: {
      char path[MAX_PATH];
      memcpy(path, param, sizeof(path));
      return fs_create_directory(path);
    }

    case SYSCALL_FS_LIST: {
      TSYSCALL_FSLIST* fi = param;
      SFS_ENTRY entry;
      FS_ENTRY o_entry;
      char path[MAX_PATH];
      uint result;
      memcpy(path, fi->path, sizeof(path));
      result = fs_list(&entry, path, fi->n);
      memcpy(o_entry.name, entry.name, sizeof(o_entry.name));
      o_entry.flags = entry.flags;
      o_entry.size = entry.size;
      memcpy(fi->entry, &o_entry, sizeof(o_entry));
      return result;
    }

    case SYSCALL_FS_FORMAT: {
      return fs_format(*(uint*)param);
    }

    case SYSCALL_DATETIME_GET: {
      io_getdatetime(param);
      return 0;
    }

    case SYSCALL_TIMER_GET: {
      return io_gettimer();
    }
  };

  return 0;
}

// The main kernel function
void kernel()
{
  // Install ISR
  install_ISR();

  debug_putstr("nano32 %u.%u build %u\n",
    OS_VERSION_HI, OS_VERSION_LO, OS_BUILD_NUM);

  // Init heap
  heap_init();

  // Init LAPIC
  lapic_init();

  // Init disks info
  io_disks_init_info();

  // Init FS info
  fs_init_info();

  // Print current disk
  debug_putstr("system disk: %2x\n",
    system_disk);

  // Show starting message
  putstr("Starting...\n");

  // Run CLI
  // This function should never return
  cli();

  // Something went wrong
  // Just spin
  while(1) {
  }
}
