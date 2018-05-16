/*
 * File system
 */
#ifndef _FS_H
#define _FS_H

/* File system specification */

/* This file system provides files, directories, pathnames and
 * bootloading facilities */

/* The file system divides disk space into logical blocks of contiguous space.
 * The size of blocks need not be the same size as the sector size of the disk
 * the file system resides on */
#define BLOCK_SIZE 512  /* block size in bytes */
/* With the current implementation, BLOCK_SIZE must be a power of 2 */

/* Disk layout: */
/* [boot block | super block | entries table | data blocks] */

/* Boot block    block 0           Boot sector */
/* Super block   block 1           Contains information about the layout of the file system */
/* Entries tab   blocks 2 to n     Table of file and directory entries */
/* Data blocks   blocks n to end   Data blocks referenced by file entries */

/* Entries are referenced by their index on the entry table
 * Entry with index n is located at byte:
 *   2*BLOCK_SIZE + n*sizeof(SFS_ENTRY)
 *
 * Data blocks start at block
 *   2 + ((superblock.nentries*sizeof(SFS_ENTRY)) / BLOCK_SIZE)
 *
 * Data blocks are referenced by their absolute disk block index
 */

/* SFS 1.0 ID used in superblock.type */
#define SFS_TYPE_ID 0x05F50010

struct  SFS_SUPERBLOCK {  /* On-disk superblock structure */
  uint32_t  type;         /* Type of file system. Must be SFS_TYPE_ID */
  uint32_t  size;         /* Total number of block in file system */
  uint32_t  nentries;     /* Number of entries in entries table */
  uint32_t  bootstart;    /* Block index of first boot program block */
};

/* The boot program must be stored in contiguous data blocks */

#define SFS_NAMESIZE    15  /* Max length of entry name + final 0 */
#define SFS_ENTRYREFS  120  /* Number of references in a single entry */

/* Entry flags */
#define T_DIR  0x01  /* Type: Directory */
#define T_FILE 0x02  /* Type: File */

#define F_USED (T_DIR | T_FILE) /* Not a flag! Used to find free entries */
/* ( (entry flags & F_USED) == 0 ) means this is a free entry */

/* fs time format: */
/* bits  0-21   second of month */
/* bits 22-31   months since 2017/01/01 00:00:00 */

struct  SFS_ENTRY {             /* On-disk entry structure */
  uint8_t   flags;              /* Entry flags. See above */
  uint8_t   name[SFS_NAMESIZE]; /* Entry name, must be finished with a 0 */
  uint32_t  time;               /* Last modification date, see format above */
  uint32_t  size;               /* File size (bytes) or number of items in a dir */
  uint32_t  parent;             /* Parent dir entry, or previous chained entry */
  uint32_t  next;               /* Next chained entry index. See below */
  uint32_t  ref[SFS_ENTRYREFS]; /* References to blocks or entries. See below */
};

/* With the current implementation, sizeof(SFS_ENTRY) must be a power of 2 */

/* References in file entries contain ordered data block indexes
 * References in directory entries contain subentries indexes
 *
 * A reference with value 0 means unused reference
 * All used references must be always packed before unused references
 *
 * Chained entries:
 * When more references than those a single entry can fit are needed,
 * entry.next contains the index of another entry for the same file or directory
 * (a chained entry) whose references are concatenated to the previous
 * entry ones. Chained entries have the same flags, name and time than their
 * head entry (the first one). An entry.next with value 0 means no more chained
 * entries
 *
 * Size in file entries contains file size in bytes
 * Size in directory entries contains the number of items in this directory
 * In both cases, size refers to the remaining size starting from the current
 * chained entry. So, the total size is only in the head entry of a file
 * or directory.
 *
 * entry.parent of the head entry of a file or directory contains the
 * index of its parent directory.
 * entry.parent of an entry which is not the first entry of a file or
 * directory (a chained entry), contains a reference to the previous entry in
 * the chain.
 *
 * The root dir of a disk is always entry index 0, with name "." and parent 0.
 * With the current implementation, the boot program must be entry index 1.
 */

 #define ROOT_DIR_NAME    "."
 #define PATH_SEPARATOR   '/'
 #define PATH_SEPARATOR_S "/"

#endif /* _FS_H */
