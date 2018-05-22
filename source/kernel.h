// Kernel

#ifndef _KERNEL_H
#define _KERNEL_H

#define OS_VERSION_HI 0
#define OS_VERSION_LO 2
#define OS_BUILD_NUM 2

// Hardware related disk information is handled by the kernel module.
// File system related information is handled by file system module
#define MAX_DISK 4

// Size of a disk sector
#define SECTOR_SIZE 512

// We need this buffer to be inside a 64KB bound
// to avoid DMA error
extern char disk_buff[SECTOR_SIZE];

struct DISKINFO {
    uint  id;          // Disk id
    char  name[4];     // Disk name
    uint  fstype;      // File system type: see ulib.h
    uint  fssize;      // Number of blocks in file system
    uint  sectors;
    uint  sides;
    uint  cylinders;
    uint  size;        // Disk size (MB)
    uint  last_access; // Last accessed time (system ms)
} disk_info[MAX_DISK];

extern uint8_t system_disk; // System disk

#endif   // _KERNEL_H
