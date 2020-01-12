// Kernel

#ifndef _KERNEL_H
#define _KERNEL_H

#define OS_VERSION_HI 1
#define OS_VERSION_LO 0
#define OS_BUILD_NUM 13

// Hardware related disk information is handled by the kernel module.
// File system related information is handled by file system module
#define MAX_DISK 4

struct DISKINFO {
    uint  id;          // Disk id
    char  name[4];     // Disk name
    char  desc[32];    // Desc
    uint  fstype;      // File system type: see ulib.h
    uint  fssize;      // Number of blocks in file system
    uint  isATA;
    uint  sectors;
    uint  sides;
    uint  cylinders;
    uint  size;        // Disk size (MB)
    uint  last_access; // Last accessed time (system ms)
} disk_info[MAX_DISK];

extern uint8_t system_disk; // System disk

#endif   // _KERNEL_H
