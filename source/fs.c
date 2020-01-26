// File system

#include "types.h"
#include "kernel.h"
#include "hwio.h"
#include "ulib/ulib.h"
#include "fs.h"

// See fs.h for more detailed description
// of file system and functions

// Disk to string
const char *disk_to_string(uint disk)
{
  if(disk < MAX_DISK) {
    return disk_info[disk].name;
  }

  return "unk";
}

// String to disk
uint string_to_disk(const char *str)
{
  for(uint index=0; index<MAX_DISK; index++) {
    if(strcmp(str, disk_info[index].name) == 0) {
      return index;
    }
  }

  return ERROR_NOT_FOUND;
}

// Return 1 if string is a disk identifier, 0 otherwise
bool string_is_disk(const char *str)
{
  for(uint index=0; index<MAX_DISK; index++) {
    if(strcmp(str, disk_info[index].name) == 0) {
      return TRUE;
    }
  }
  return FALSE;
}

// Convert number of blocks to size in MB
uint blocks_to_MB(uint blocks)
{
  return (blocks*BLOCK_SIZE)/1048576;
}

// Convert a user input entry name to a valid name.
// Bad symbols are removed from input str and lenght
// is clamped to SFS_NAMESIZE-1
static char *string_to_name(char *str)
{
  for(uint i=0; i<strlen(str); i++) {
    if((str[i] < '0' || str[i] > '9') &&
       (str[i] < 'A' || str[i] > 'Z') &&
       (str[i] < 'a' || str[i] > 'z') &&
      str[i] != '.' &&
      str[i] != '-' && str[i] != '_' &&
      str[i] != '(' && str[i] != ')' &&
      str[i] != '[' && str[i] != ']') {
      str[i] = '_';
    }
    if(i == SFS_NAMESIZE-1) {
      str[i] = 0;
      break;
    }
  }
  return str;
}

// Read disk, specific block, offset and size
// Returns NO_ERROR on success, another value otherwise
static uint read_disk(uint disk, uint block, uint offset, size_t size, void *buff)
{
  // Convert blocks to sectors
  uint sector = (block * BLOCK_SIZE) / DISK_SECTOR_SIZE;

  // Compute initial sector and offset
  sector += offset / DISK_SECTOR_SIZE;
  offset = offset % DISK_SECTOR_SIZE;

  return io_disk_read(disk, sector, offset, size, buff);
}

// Write disk, specific sector, offset and size
// Returns NO_ERROR on success, another value otherwise
static uint write_disk(uint disk, uint block, uint offset, size_t size, const void *buff)
{
  // Convert blocks to sectors
  uint sector = (block * BLOCK_SIZE) / DISK_SECTOR_SIZE;

  // Compute initial sector and offset
  sector += offset / DISK_SECTOR_SIZE;
  offset = offset % DISK_SECTOR_SIZE;

  return io_disk_write(disk, sector, offset, size, buff);
}

// Get filesystem info
uint fs_get_info(uint disk, fs_info_t *info)
{
  // Check parameters
  if(info == 0 || disk >= MAX_DISK) {
    return 1;
  }

  // Count avaliable disks (n)
  uint n = 0;
  uint j = 0xFFFF;
  for(uint i=0; i<MAX_DISK; i++) {
    if(disk_info[i].size != 0) {
      if(n == disk) {
        j = disk;
      }
      n++;
    }
  }

  // Fill info
  if(j != 0xFFFF && disk_info[j].size != 0) {
    info->id = j;
    strncpy(info->name, disk_to_string(info->id), sizeof(info->name));
    info->fs_type = disk_info[j].fstype;
    info->fs_size = blocks_to_MB(disk_info[j].fssize);
    info->disk_size = disk_info[j].size;
  }

  return n;
}

// Init file system info
// Reads superblock and fills disk info
void fs_init_info()
{
  // For each disk
  for(uint disk_index=0; disk_index<MAX_DISK; disk_index++) {
    debug_putstr("Check filesystem in %2x: ", disk_index);

    // If hardware related disk info is valid
    if(disk_info[disk_index].size != 0) {
      // Read superblock and check file system type and data
      sfs_superblock_t sb;
      const uint result = read_disk(disk_index, 1, 0, sizeof(sb), &sb);
      if(result == NO_ERROR && sb.type == SFS_TYPE_ID) {
        disk_info[disk_index].fstype = FS_TYPE_NSFS;
        disk_info[disk_index].fssize = sb.size;
        debug_putstr("NSFS fssize=%u blocks size=%uMB %s\n",
          disk_info[disk_index].fssize, disk_info[disk_index].size,
          disk_info[disk_index].isATA?"ATA":"");
        continue;
      }
    }
    disk_info[disk_index].fstype = FS_TYPE_UNKNOWN;
    disk_info[disk_index].fssize = 0;
    debug_putstr("unknown\n");
  }
}

// Get disk id from path string
static uint path_get_disk(const char *path)
{
  // Make a copy of path because tokenize
  // process will replaces some bytes
  char tokpath[64] = {0};
  memset(tokpath, 0, sizeof(tokpath));
  strncpy(tokpath, path, sizeof(tokpath));
  char *tok = tokpath;
  char *nexttok = tok;

  // First token should be disk identifier
  tok = strtok(tok, &nexttok, PATH_SEPARATOR);
  if(*tok) {
    if(string_is_disk(tok)) {
      return string_to_disk(tok);
    }
  }

  // Return system disk if disk is unspecified
  return system_disk;
}

// Given an entry full path (path), this functions computes disk, parent entry index, and
// a pointer to the start of the name of the entry in the input path.
// The parent path must exist, but full path entry can not exist.
// Return NO_ERROR on success, ERROR_NOT_FOUND if parent path does not exist, 
// or another error code otherwise
static uint path_parse_disk_parent_name(char **name, uint *parent, uint *disk, char *path)
{
  // Make a copy of path because tokenize
  // process will replaces some bytes
  char tokpath[64] = {0};
  memset(tokpath, 0, sizeof(tokpath));
  strncpy(tokpath, path, sizeof(tokpath));
  char *tok = tokpath;
  char *nexttok = tok;

  // Start at root of system disk
  *disk = system_disk;
  *parent = 0;
  *name = path;

  // Check entries token by token
  uint n = 0;
  while(*tok && *nexttok) {
    tok = strtok(tok, &nexttok, PATH_SEPARATOR);
    *name = path + (uint)(tok - tokpath);

    // First path token can be disk identifier
    if(*tok && n == 0) {
      n++;
      if(string_is_disk(tok)) {
        *disk = string_to_disk(tok);
        tok = nexttok;
        continue;
      }
    }
    // Check entry exists
    if(*tok && *nexttok) {
      sfs_entry_t entry;
      *parent = fs_get_entry(&entry, tok, *parent, *disk);
      if(*parent >= ERROR_ANY) {
        return *parent;
      }
      tok = nexttok;
    }
  }

  return NO_ERROR;
}

// Get entry by disk and index
// Returns the input index or ERROR_IO. Does check nothing
static uint get_entry_n(sfs_entry_t *entry, uint disk, size_t n)
{
  // Compute block number and offset
  const uint block = 2 + (n * sizeof(sfs_entry_t)) / BLOCK_SIZE;
  const uint offset = (n * sizeof(sfs_entry_t)) % BLOCK_SIZE;

  // Read and return
  uint result = read_disk(disk, block, offset,
    sizeof(sfs_entry_t), (char*)entry);

  return result != NO_ERROR ? ERROR_IO : n;
}

// Get an entry given a path, parent and disk
uint fs_get_entry(sfs_entry_t *entry, char *path, uint parent, uint disk)
{
  // If path is just a disk identifier and disk is unknown
  // then path is the root directory of this disk
  if(disk == UNKNOWN_VALUE && string_is_disk(path)) {
    disk = string_to_disk(path);
    path = ROOT_DIR_NAME;
  }
  // If parent is unknown, start at root directory
  if(parent == UNKNOWN_VALUE) {
    parent = 0;
  }
  // If disk is unknown, assume system disk
  if(disk == UNKNOWN_VALUE) {
    disk = system_disk;
  }

  uint result = NO_ERROR;

  // If path has components, get parent, disk and single entry name
  if(parent == 0 && strchr(path, PATH_SEPARATOR)) {
    char *name = path;
    result = path_parse_disk_parent_name(&name, &parent, &disk, path);
    if(result >= ERROR_ANY) {
      return result;
    }
    path = name;
  }

  // Read superblock
  sfs_superblock_t sb;
  result = read_disk(disk, 1, 0, sizeof(sb), (void*)&sb);
  if(result != NO_ERROR) {
    return ERROR_IO;
  }

  // Check all entries
  uint n = 0;
  while(n < sb.nentries) {
    result = get_entry_n(entry, disk, n);
    if(result >= ERROR_ANY) {
      return result;
    }
    if((entry->flags & F_USED) && entry->parent == parent &&
      !strcmp((char*)entry->name, path)) {
      return n;
    }
    n++;
  }

  return ERROR_NOT_FOUND;
}

// When the number of references in an entry is not enough to fit contents,
// a chained entry for the same file or directory is created.
//
// Given the first entry of a file or directory (entry), and a reference
// index (nref), this function finds the chained entry (outentry) which
// contains the reference with index nref.
// disk id and index of input entry must be also provided.
// Returns the index of outentry in the entry table, or an error code
static uint get_nref_entry_from_entry(sfs_entry_t *outentry,
  sfs_entry_t *entry, uint disk, uint nentry, uint nref)
{
  // Initialize return entry information to the first entry
  uint result = nentry;
  if(outentry != entry) {
    memcpy(outentry, entry, sizeof(sfs_entry_t));
  }

  // While reference index exceeds number of references in an entry
  while(nref >= SFS_ENTRYREFS) {
    nref -= SFS_ENTRYREFS;
    if(outentry->next) {
      // Advance to the next chained entry
      result = get_entry_n(outentry, disk, outentry->next);
      if(result >= ERROR_ANY) {
        return result;
      }
    } else {
      return ERROR_NOT_FOUND;
    }
  }
  return result;
}

// Read file in buff, given path, offset and count
uint fs_read_file(void *buff, char *path, uint offset, size_t count)
{
  debug_putstr("fs_read_file %x %s %u %u\n", buff, path, offset, count);
  uint result = NO_ERROR;

  // Find entry
  sfs_entry_t entry;
  sfs_entry_t tentry;
  uint nentry = fs_get_entry(&entry, path, UNKNOWN_VALUE, UNKNOWN_VALUE);
  if(nentry < ERROR_ANY && (entry.flags & T_FILE)) {
    // Compute initial block and offset
    offset = min(offset, entry.size);
    count = min(count, entry.size - offset);
    uint block = offset / BLOCK_SIZE;
    offset = offset % BLOCK_SIZE;
    const uint disk = path_get_disk(path);
    uint read = 0;
    uint ntentry = nentry;
    while(read < count) {
      // Get chained entry for a given reference index number
      ntentry = get_nref_entry_from_entry(&tentry, &entry, disk, nentry, block);
      if(ntentry >= ERROR_ANY) {
        return ntentry;
      }

      // Read in buffer
      const size_t n = min(BLOCK_SIZE-offset, count-read);
      result = read_disk(disk, tentry.ref[block % SFS_ENTRYREFS], offset,
        n, buff + read);

      if(result != NO_ERROR) {
        return ERROR_IO;
      }

      read += n;
      block++;
      offset = 0;
    }
    result = read;
  } else {
    if(nentry < ERROR_ANY) {
      result = ERROR_NOT_FOUND;
    } else {
      result = nentry;
    }
  }

  return result;
}

// Write entry by index at disk
static uint write_entry(sfs_entry_t *entry, uint disk, size_t n)
{
  // Compute block number and offset
  const uint block = 2 + (n*sizeof(sfs_entry_t)) / BLOCK_SIZE;
  const uint offset = (n*sizeof(sfs_entry_t)) % BLOCK_SIZE;

  // Write and return
  uint result = write_disk(disk, block, offset,
    sizeof(sfs_entry_t), entry);

  return result != NO_ERROR ? ERROR_IO : NO_ERROR;
}

// Set entry time to NOW
static uint set_entry_time_to_current(uint disk, uint nentry)
{
  // Get time and convert to fs time
  time_t ctime = {0};
  get_datetime(&ctime);
  const uint32_t fstime = fs_systime_to_fstime(&ctime);

  // Set in initial entry and all chained entries
  do {
    sfs_entry_t entry;
    uint result = get_entry_n(&entry, disk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
    entry.time = fstime;
    result = write_entry(&entry, disk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
    if(nentry == entry.next && nentry != 0) {
      debug_putstr("set_entry_time error: nentry=%u entry.next=%u\n",
        nentry, entry.next);
      return ERROR_IO;
    }
    nentry = entry.next;
  } while(nentry != 0);

  return NO_ERROR;
}

// Find first free entry in disk
// Return its index or an error code
static uint find_free_entry(uint disk)
{
  // Read super block
  sfs_superblock_t sb;
  uint result = read_disk(disk, 1, 0, sizeof(sb), &sb);
  if(result != NO_ERROR) {
    return ERROR_IO;
  }

  // Find an unused entry
  uint n = 0;
  while(n < sb.nentries) {
    sfs_entry_t entry;
    result = get_entry_n(&entry, disk, n);
    if(result >= ERROR_ANY) {
      return result;
    }
    if(!(entry.flags & F_USED)) {
      return n;
    }
    n++;
  }

  return ERROR_NO_SPACE;
}

// Get number of needed blocks to contain a given size (bytes)
static size_t needed_blocks(size_t size)
{
  size_t nblocks = size / BLOCK_SIZE;
  if(size % BLOCK_SIZE) {
    nblocks++;
  }
  return nblocks;
}

// Find first free block at disk
// Return its index or ERROR_NOT_FOUND
static uint find_free_block(uint disk)
{
  // Read superblock
  sfs_superblock_t sb;
  uint result = read_disk(disk, 1, 0, sizeof(sb), &sb);
  if(result != NO_ERROR) {
    return ERROR_IO;
  }

  // Compute first data block index
  const uint first_data_block = 2 +
    (sb.nentries*sizeof(sfs_entry_t))/BLOCK_SIZE;

  const uint max_blocks = sb.size;

  // For each possible block index
  for(uint free_block=first_data_block; free_block<max_blocks; free_block++) {
    // Check if there is an entry referencing it
    uint found = 0;
    uint n = 0;
    while(found == 0 && n < sb.nentries) {
      sfs_entry_t entry;
      result = get_entry_n(&entry, disk, n);
      if(result >= ERROR_ANY) {
        return result;
      }
      if(entry.flags & T_FILE) {
        const uint max_block = min(needed_blocks(entry.size), SFS_ENTRYREFS);
        for(uint b=0; b<max_block; b++) {
          if(entry.ref[b] == free_block) {
            // If there is, this block is not free
            found = 1;
            break;
          }
        }
      }
      n++;
    }
    if(found == 0) {
      // Otherwise, this is a free block
      return free_block;
    }
  }

  debug_putstr("find_free_block: error: no space\n");
  return ERROR_NO_SPACE;
}

// Set references count in an entry
//
// Given an initial entry index, this function creates new chained entries
// or deletes existing ones to fit a given number of total references.
// Unused or newly created references are set to 0
static uint set_entry_refcount(uint disk, uint nentry, uint refcount)
{
  // Compute number of needed entries
  uint nentries = refcount / SFS_ENTRYREFS;
  if(refcount % SFS_ENTRYREFS) {
    nentries += 1;
  }

  // Start from the first one
  sfs_entry_t entry;
  uint result = get_entry_n(&entry, disk, nentry);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Advance to the last chained entry, create if needed
  while(nentries > 1) {
    if(entry.next) {
      nentry = get_entry_n(&entry, disk, entry.next);
      if(nentry >= ERROR_ANY) {
        return nentry;
      }
    } else {
      entry.next = find_free_entry(disk);
      if(entry.next >= ERROR_ANY) {
        return entry.next;
      }
      result = write_entry(&entry, disk, nentry);
      if(result >= ERROR_ANY) {
        return result;
      }
      entry.parent = nentry;
      nentry = entry.next;
      entry.next = 0;
      entry.size = 0;
      memset(entry.ref, 0, sizeof(entry.ref));
      result = write_entry(&entry, disk, nentry);
      if(result >= ERROR_ANY) {
        return result;
      }
    }
    nentries--;
  }

  // Set to 0 unused references of last chained entry
  const uint first_unused = min(SFS_ENTRYREFS-1,refcount)%SFS_ENTRYREFS;
  for(uint i=first_unused; i<SFS_ENTRYREFS; i++) {
    entry.ref[i] = 0;
  }
  result = write_entry(&entry, disk, nentry);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Delete remaining chained entries
  if(entry.next) {
    uint current = nentry;
    uint next = entry.next;
    entry.next = 0;
    result = write_entry(&entry, disk,current);
    if(result >= ERROR_ANY) {
      return result;
    }
    do {
      current = get_entry_n(&entry, disk, next);
      if(current >= ERROR_ANY) {
        return current;
      }
      next = entry.next;
      memset(&entry, 0, sizeof(entry));
      result = write_entry(&entry, disk, current);
      if(result >= ERROR_ANY) {
        return result;
      }
    } while(next);
  }

  return NO_ERROR;
}

// Set entry size value, and update also chained entries
static uint set_entry_size(uint disk, uint nentry, size_t size)
{
  // Update chain starting from nentry
  do {
    sfs_entry_t entry;
    uint result = get_entry_n(&entry, disk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
    entry.size = size;
    if(entry.flags & T_FILE) {
      size -= min(size, SFS_ENTRYREFS * BLOCK_SIZE);
    } else if(entry.flags & T_DIR) {
      size -= min(size, SFS_ENTRYREFS);
    }

    result = write_entry(&entry, disk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
    nentry = entry.next;

    if((size > 0 && nentry == 0) ||
      (size == 0 && nentry != 0)) {
      debug_putstr("set_entry_size error; size=%u nentry=%u\n",
        size, nentry);
      return ERROR_IO;
    }
  } while(nentry != 0);

  return NO_ERROR;
}

// Given an entry, return its refcount
static uint get_entry_refcount(sfs_entry_t *entry)
{
  uint refcount = 0;
  if(entry->flags & T_FILE) {
    refcount = needed_blocks(entry->size);
  } else if(entry->flags & T_DIR) {
    refcount = entry->size;
  }

  return refcount;
}

// Add a reference in an entry
// Advances throguh the chain if needed
static uint add_ref_in_entry(uint disk, uint nentry, uint ref)
{
  // Get initial entry
  sfs_entry_t entry;
  uint result = get_entry_n(&entry, disk, nentry);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Get current number of references
  uint refcount = get_entry_refcount(&entry);

  // Get chained entry to add a new one
  sfs_entry_t refentry;
  uint nrefentry = get_nref_entry_from_entry(&refentry, &entry, disk, nentry, refcount);
  if(nrefentry>=ERROR_ANY && nrefentry!=ERROR_NOT_FOUND) {
    return nrefentry;
  }

  // Create if does not exist
  if(nrefentry == ERROR_NOT_FOUND) {
    nrefentry = set_entry_refcount(disk, nentry, refcount + 1);
    if(nrefentry >= ERROR_ANY) {
      return nrefentry;
    }
    result = get_entry_n(&refentry, disk, nrefentry);
    if(result >= ERROR_ANY) {
      return result;
    }
  }

  // Set the reference
  refentry.ref[refcount % SFS_ENTRYREFS] = ref;
  result = write_entry(&refentry, disk, nrefentry);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Update size if it's a directory
  if(entry.flags & T_DIR) {
    result = set_entry_size(disk, nentry, entry.size + 1);
    if(result >= ERROR_ANY) {
      return result;
    }
  }

  return NO_ERROR;
}

// Remove a reference in an entry
// Updates size and chain length if needed
static uint remove_ref_in_entry(uint disk, uint nentry, uint ref)
{
  // Get initial entry
  sfs_entry_t entry;
  uint result = get_entry_n(&entry, disk, nentry);
  if(result >= ERROR_ANY) {
    return result;
  }
  sfs_entry_t currentry;
  sfs_entry_t nextentry;
  memcpy(&currentry, &entry, sizeof(entry));
  memcpy(&nextentry, &entry, sizeof(entry));

  // Get total number of references
  uint refcount = get_entry_refcount(&entry);

  // Find value in reference list
  char found = 0;
  uint ncurrentry = nentry;
  for(uint r=0; r<refcount; r++) {
    if(r == SFS_ENTRYREFS) {
      result = write_entry(&currentry, disk, ncurrentry);
      if(result >= ERROR_ANY) {
        return result;
      }
      ncurrentry = get_entry_n(&currentry, disk, currentry.next);
      if(ncurrentry >= ERROR_ANY) {
        return ncurrentry;
      }
      r = 0;
      refcount = get_entry_refcount(&currentry);
    } else if(r + 1 == SFS_ENTRYREFS) {
      if(nextentry.next) {
        result = get_entry_n(&nextentry, disk, nextentry.next);
        if(result >= ERROR_ANY) {
          return result;
        }
      } else {
        memset(&nextentry, 0, sizeof(nextentry));
      }
    }

    if(!found && currentry.ref[r] == ref) {
      found = 1;
    }
    if(found) {
      currentry.ref[r] = nextentry.ref[(r + 1) % SFS_ENTRYREFS];
    }
  }

  // If found, update
  if(found) {
    result = write_entry(&currentry, disk, ncurrentry);
    if(result >= ERROR_ANY) {
      return result;
    }
    result = set_entry_refcount(disk, nentry, refcount - 1);
    if(result >= ERROR_ANY) {
      return result;
    }
    if(entry.flags & T_DIR) {
      result = set_entry_size(disk, nentry, refcount - 1);
      if(result >= ERROR_ANY) {
        return result;
      }
    }
  }

  return NO_ERROR;
}

// Write buff to file given path, offset, count and flags
uint fs_write_file(const void *buff, char *path, uint offset, size_t count, uint flags)
{
  debug_putstr("fs_write_file %x %s %u %u\n", buff, path, offset, count);

  // Find file
  sfs_entry_t entry;
  uint nentry = fs_get_entry(&entry, path, UNKNOWN_VALUE, UNKNOWN_VALUE);

  // Does not exist and should not create or it's a directory: return
  if((nentry == ERROR_NOT_FOUND && !(flags & WF_CREATE)) ||
    (nentry >= ERROR_ANY && nentry != ERROR_NOT_FOUND)) {
    return nentry;
  }
  if(nentry < ERROR_ANY && (entry.flags & T_DIR)) {
    return ERROR_NOT_FOUND;
  }

  // Create file if needed
  uint result = NO_ERROR;
  uint disk = path_get_disk(path);
  if(nentry == ERROR_NOT_FOUND && (flags & WF_CREATE)) {
    uint parent = 0;
    memset(&entry, 0, sizeof(entry));

    // Parse parent, disk and name
    result = path_parse_disk_parent_name(&path, &parent, &disk, path);
    if(result >= ERROR_ANY) {
      return result;
    }

    // Make name a valid name
    path = string_to_name(path);

    // Fill entry data
    nentry = find_free_entry(disk);
    if(nentry >= ERROR_ANY) {
      return nentry;
    }
    entry.size = 0;
    entry.next = 0;
    entry.parent = parent;
    entry.flags = T_FILE;
    strncpy((char*)entry.name, path, SFS_NAMESIZE);
    result = write_entry(&entry, disk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }

    // Add reference in parent
    result = add_ref_in_entry(disk, entry.parent, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
  }

  // Resize: grow if needed
  if(entry.size < offset + count) {
    uint current_num_blocks = needed_blocks(entry.size);
    uint required_num_blocks = needed_blocks(offset + count);

    // Set reference count and size in the chain
    result = set_entry_refcount(disk, nentry, required_num_blocks);
    if(result >= ERROR_ANY) {
      return result;
    }
    result = set_entry_size(disk, nentry, offset + count);
    if(result >= ERROR_ANY) {
      return result;
    }

    // Allocate blocks
    sfs_entry_t tentry;
    result = get_entry_n(&tentry, disk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
    uint ntentry = nentry;

    for(; current_num_blocks < required_num_blocks; current_num_blocks++) {
      while(current_num_blocks >= SFS_ENTRYREFS) {
        result = write_entry(&tentry, disk, ntentry);
        if(result >= ERROR_ANY) {
          return result;
        }
        ntentry = get_entry_n(&tentry, disk, tentry.next);
        if(ntentry >= ERROR_ANY) {
          return ntentry;
        }
        current_num_blocks -= SFS_ENTRYREFS;
        required_num_blocks -= SFS_ENTRYREFS;
      }
      tentry.ref[current_num_blocks] = find_free_block(disk);
      if(tentry.ref[current_num_blocks] >= ERROR_ANY) {
        return tentry.ref[current_num_blocks];
      }
    }
    result = write_entry(&tentry, disk, ntentry);
    if(result >= ERROR_ANY) {
      return result;
    }
    result = get_entry_n(&entry, disk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
  }

  // Resize: shrink if needed
  if(entry.size > offset + count && (flags & WF_TRUNCATE)) {
    result = set_entry_refcount(disk, nentry, needed_blocks(offset + count));
    if(result >= ERROR_ANY) {
      return result;
    }
    result = set_entry_size(disk, nentry, offset + count);
    if(result >= ERROR_ANY) {
      return result;
    }
    result = get_entry_n(&entry, disk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
  }

  // Now file has the right size: write data
  uint written = 0;
  while(count > 0) {
    uint to_copy = min(count, BLOCK_SIZE - (offset % BLOCK_SIZE));
    uint current_block = needed_blocks(offset+1) - 1;
    while(current_block >= SFS_ENTRYREFS) {
      if(entry.next == 0) {
        debug_putstr("fs_write_file error: current_block=%u entry.next=%u\n",
          current_block, entry.next);
        return ERROR_IO;
      }
      result = get_entry_n(&entry, disk, entry.next);
      if(result >= ERROR_ANY) {
        return result;
      }
      offset -= SFS_ENTRYREFS*BLOCK_SIZE;
      current_block -= SFS_ENTRYREFS;
    }

    // Check for error (try to write in system blocks)
    if(entry.ref[current_block]==0 || entry.ref[current_block]==1) {
      debug_putstr("fs_write_file error: entryref=%u current_block=%u\n",
        entry.ref[current_block], current_block);
      return ERROR_IO;
    }
    result = write_disk(disk, entry.ref[current_block],
      offset%BLOCK_SIZE, to_copy, buff + written);
    if(result != NO_ERROR) {
      return result;
    }
    count -= to_copy;
    offset += to_copy;
    written += to_copy;
  }

  // Update file entry time
  result = set_entry_time_to_current(disk, nentry);
  if(result >= ERROR_ANY) {
    return result;
  }

  return written;
}

// Delete entry by index
// Deletes the full chain
static uint delete_n(uint disk, size_t n)
{
  sfs_entry_t entry;
  uint result = get_entry_n(&entry, disk, n);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Delete reference in parent
  result = remove_ref_in_entry(disk, entry.parent, n);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Recursively delete all files if it's a directory
  if(entry.flags & T_DIR) {
    uint b = 0;
    while(b < min(entry.size, SFS_ENTRYREFS)) {
      result = delete_n(disk, entry.ref[b]);
      if(result >= ERROR_ANY) {
        return result;
      }
      b++;
    }
  }

  // Delete full chain
  if(entry.next) {
    result = delete_n(disk, entry.next);
    if(result >= ERROR_ANY) {
      return result;
    }
  }

  memset(&entry, 0, sizeof(entry));
  result = write_entry(&entry, disk, n);
  if(result >= ERROR_ANY) {
    return result;
  }

  return NO_ERROR;
}

// Delete entry by path
uint fs_delete(char *path)
{
  // Find entry, and delete by index
  uint disk = path_get_disk(path);
  sfs_entry_t entry;
  uint nentry = fs_get_entry(&entry, path, UNKNOWN_VALUE, UNKNOWN_VALUE);
  if(nentry < ERROR_ANY) {
    nentry = delete_n(disk, nentry);
  }

  return nentry;
}

// Create a dirrectory
uint fs_create_directory(char *path)
{
  // Get disk, parent entry index, and directory name from path
  uint disk = UNKNOWN_VALUE;
  uint parent = UNKNOWN_VALUE;
  uint result = path_parse_disk_parent_name(&path, &parent, &disk, path);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Make name a valid name
  path = string_to_name(path);

  // Check target does not exist
  sfs_entry_t entry;
  result = fs_get_entry(&entry, path, parent, disk);
  if(result != ERROR_NOT_FOUND) {
    if(result >= ERROR_ANY) {
      return result;
    }
    return ERROR_EXISTS;
  }

  // Find a free entry index
  uint nentry = find_free_entry(disk);
  if(nentry >= ERROR_ANY) {
    return nentry;
  }

  // Create entry
  memset(&entry, 0, sizeof(entry));
  strncpy((char*)entry.name, path, SFS_NAMESIZE);
  entry.size = 0;
  entry.flags = T_DIR;
  entry.parent = parent;
  entry.next = 0;
  result = write_entry(&entry, disk, nentry);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Update modified time
  result = set_entry_time_to_current(disk, nentry);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Add reference in parent
  if(nentry != entry.parent) {
    result = add_ref_in_entry(disk, entry.parent, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
  }

  return nentry;
}

// Move entry
uint fs_move(char *srcpath, char *dstpath)
{
  // Get disk, parent entry index, and name from destination path
  uint dst_parent=0, dstdisk=0;
  char *dstname = 0;
  uint result = path_parse_disk_parent_name(&dstname, &dst_parent, &dstdisk, dstpath);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Make name a valid name
  dstname = string_to_name(dstname);

  // Check destination does not exist
  sfs_entry_t entry;
  result = fs_get_entry(&entry, dstname, dst_parent, dstdisk);
  if(result != ERROR_NOT_FOUND) {
    if(result >= ERROR_ANY) {
      return result;
    }
    return ERROR_EXISTS;
  }

  // Check source exists
  const uint srcdisk = path_get_disk(srcpath);
  const uint nentry = fs_get_entry(&entry, srcpath, UNKNOWN_VALUE, UNKNOWN_VALUE);
  if(nentry >= ERROR_ANY) {
    return nentry;
  }

  // If moving between disks, physically move data: copy and delete
  if(srcdisk != dstdisk) {
    result = fs_copy(srcpath, dstpath);
    if(result != NO_ERROR) {
      return result;
    }
    result = delete_n(srcdisk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
  } else {
    // Else, remove reference in parent
    result = remove_ref_in_entry(srcdisk, entry.parent, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }

    // Update entry parent and name
    memset(entry.name, 0, sizeof(entry.name));
    strncpy((char*)entry.name, dstname, SFS_NAMESIZE);
    entry.parent = dst_parent;
    result = write_entry(&entry, dstdisk, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }

    // Add reference in parent
    result = add_ref_in_entry(dstdisk, entry.parent, nentry);
    if(result >= ERROR_ANY) {
      return result;
    }
  }

  return nentry;
}

// Copy entry
uint fs_copy(char *srcpath, char *dstpath)
{
  // Get disk, parent entry index, and name from destination path
  uint dst_parent=0, dst_disk=0;
  char *dstname = 0;
  uint result = path_parse_disk_parent_name(&dstname, &dst_parent, &dst_disk, dstpath);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Make name a valid name
  dstname = string_to_name(dstname);

  // Check destination does not exist
  sfs_entry_t entry;
  result = fs_get_entry(&entry, dstname, dst_parent, dst_disk);
  if(result != ERROR_NOT_FOUND) {
    if(result >= ERROR_ANY) {
      return result;
    }
    return ERROR_EXISTS;
  }

  // Check source exists
  const uint src_disk = path_get_disk(srcpath);
  const uint nentry = fs_get_entry(&entry, srcpath, UNKNOWN_VALUE, UNKNOWN_VALUE);
  if(nentry >= ERROR_ANY) {
    return nentry;
  }

  // If source is a file, just read and write the full file
  if(entry.flags & T_FILE) {
    uint offset = 0;
    uint copied = 0;
    char buff[BLOCK_SIZE] = {0};

    while((copied = fs_read_file(buff, srcpath, offset, sizeof(buff)))) {
      if(copied >= ERROR_ANY) {
        return copied;
      }
      result = fs_write_file(buff, dstpath, offset, copied, WF_CREATE);
      if(result >= ERROR_ANY) {
        return result;
      }
      offset += copied;
    }
    return NO_ERROR;
  }
  // If source is a directory
  else if(entry.flags & T_DIR) {
    // Create the directory
    result = fs_create_directory(dstpath);
    if(result >= ERROR_ANY) {
      return result;
    }

    // Iterate and recursively copy entries
    for(uint r=0; r<entry.size; r++) {
      sfs_entry_t tentry;
      char src_path[64] = {0};
      char dst_path[64] = {0};
      result = get_entry_n(&tentry, src_disk, entry.ref[r]);
      if(result >= ERROR_ANY) {
        return result;
      }

      // Compute src and dest paths, and move
      strncpy(src_path, srcpath, sizeof(src_path));
      strncat(src_path, PATH_SEPARATOR_S, sizeof(src_path));
      strncat(src_path, (char*)tentry.name, sizeof(src_path));

      strncpy(dst_path, dstpath, sizeof(src_path));
      strncat(dst_path, PATH_SEPARATOR_S, sizeof(dst_path));
      strncat(dst_path, (char*)tentry.name, sizeof(dst_path));

      result = fs_copy(src_path, dst_path);
      if(result >= ERROR_ANY) {
        return result;
      }
    }
    return NO_ERROR;
  }

  return ERROR_NOT_FOUND;
}

// List entries in a directory
uint fs_list(sfs_entry_t *entry, char *path, uint n)
{
  memset(entry, 0, sizeof(sfs_entry_t));

  // Find entry
  sfs_entry_t direntry;
  const uint nentry = fs_get_entry(&direntry, path, UNKNOWN_VALUE, UNKNOWN_VALUE);
  if(nentry >= ERROR_ANY) {
    return nentry;
  }

  // If found, and it's a directory
  if(direntry.flags & T_DIR) {
    if(n < direntry.size) {
       // Advance to the chained entry contaning the nth reference
      const uint disk = path_get_disk(path);
      uint res = get_nref_entry_from_entry(&direntry, &direntry, disk, nentry, n);
      if(res >= ERROR_ANY) {
        return res;
      }
      // Get the entry
      res = get_entry_n(entry, disk, direntry.ref[n]);
      if(res >= ERROR_ANY) {
        return res;
      }
    }
    return direntry.size;
  }
  return ERROR_NOT_FOUND;
}

// Print FS map
uint fs_print_map(char *filename)
{
  // Find entry
  uint result = NO_ERROR;
  sfs_entry_t entry;
  sfs_entry_t tentry;
  debug_putstr("FS map for file %s\n", filename);
  uint nentry = fs_get_entry(&entry, filename, UNKNOWN_VALUE, UNKNOWN_VALUE);
  if(nentry < ERROR_ANY && (entry.flags & T_FILE)) {
    // Compute initial block and offset
    const uint disk = path_get_disk(filename);
    uint block = 0;
    uint read = 0;
    uint ntentry = nentry;
    while(read < entry.size) {
      // Get chained entry for a given reference index number
      ntentry = get_nref_entry_from_entry(&tentry, &entry, disk, nentry, block);
      if(ntentry >= ERROR_ANY) {
        debug_putstr("fs_print_map: error getting entry\n");
        return ntentry;
      }

      debug_putstr("entry: %u entry.next: %u block: %u\n", ntentry, tentry.next, block);
      for(uint i=0; i<SFS_ENTRYREFS; i++) {
        debug_putstr("b:%u ", tentry.ref[i]);
        if(i%12 == 11 || i==SFS_ENTRYREFS-1) {
          debug_putstr("\n");
        }
      }
      read += SFS_ENTRYREFS * BLOCK_SIZE;
      block += SFS_ENTRYREFS;
    }
    result = read;
  } else {
    if(nentry < ERROR_ANY) {
      result = ERROR_NOT_FOUND;
    } else {
      result = nentry;
    }
  }
  debug_putstr("FS map finished\n");

  return result;
}

// Format a disk
uint fs_format(uint disk)
{
  debug_putstr("format disk: %2x (system_disk=%2x)\n",
    disk, system_disk);

  // Copy boot block from system disk to target disk
  char buff[BLOCK_SIZE] = {0};
  uint result = read_disk(system_disk, 0, 0, BLOCK_SIZE, buff);
  if(result != 0) {
    return ERROR_IO;
  }

  result = write_disk(disk, 0, 0, BLOCK_SIZE, buff);
  if(result != 0) {
    return ERROR_IO;
  }

  // Create superblock
  uint32_t disk_size = disk_info[disk].sectors *
    disk_info[disk].sides *
    disk_info[disk].cylinders;

  if(DISK_SECTOR_SIZE > BLOCK_SIZE) {
    disk_size *= (DISK_SECTOR_SIZE/BLOCK_SIZE);
  } else {
    disk_size /= (BLOCK_SIZE/DISK_SECTOR_SIZE);
  }

  memset(buff, 0, sizeof(buff));
  sfs_superblock_t *sb = (sfs_superblock_t*)buff;
  sb->type = SFS_TYPE_ID;
  sb->size = disk_size;
  sb->nentries = min(
    (((sb->size * BLOCK_SIZE)/10)/sizeof(sfs_entry_t)),
    1024);
  sb->bootstart = 2 + (sb->nentries * sizeof(sfs_entry_t)) / BLOCK_SIZE;
  result = write_disk(disk, 1, 0, BLOCK_SIZE, sb);
  if(result != 0) {
    return ERROR_IO;
  }
  debug_putstr("format: disk=%2x blocks=%u entries=%u boot=%u\n",
    disk, sb->size, sb->nentries, sb->bootstart);

  const uint nentries = (uint)sb->nentries;

  // Create root dir
  memset(buff, 0, sizeof(buff));
  sfs_entry_t *entry = (sfs_entry_t*)buff;
  strncpy((char*)entry->name, ROOT_DIR_NAME, SFS_NAMESIZE);
  entry->flags = T_DIR;
  entry->size = 0;
  entry->parent = 0;
  entry->next = 0;
  entry->time = 0;

  result = write_entry(entry, disk, 0);
  if(result >= ERROR_ANY) {
    return result;
  }

  result = set_entry_time_to_current(disk, 0);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Write empty entries table
  memset(buff, 0, sizeof(buff));
  for(uint e=1; e<nentries; e++) {
    result = write_entry(entry, disk, e);
    if(result >= ERROR_ANY) {
      return result;
    }
  }

  // Copy boot program
  result = get_entry_n(entry, system_disk, 1);
  if(result >= ERROR_ANY) {
    return result;
  }

  char k_src_path[32] = {0};
  char k_dst_path[32] = {0};
  strncpy(k_src_path, disk_to_string(system_disk), sizeof(k_src_path));
  strncat(k_src_path, PATH_SEPARATOR_S, sizeof(k_src_path));
  strncat(k_src_path, (char*)entry->name, sizeof(k_src_path));
  strncpy(k_dst_path, disk_to_string(disk), sizeof(k_dst_path));
  strncat(k_dst_path, PATH_SEPARATOR_S, sizeof(k_dst_path));
  strncat(k_dst_path, (char*)entry->name, sizeof(k_dst_path));

  debug_putstr("format: copy %s %s\n", k_src_path, k_dst_path);

  result = fs_copy(k_src_path, k_dst_path);
  if(result >= ERROR_ANY) {
    return result;
  }

  // Update disks file system information
  fs_init_info();

  return result;
}

// Convert fs-time to system TIME
uint fs_fstime_to_systime(uint32_t fst, time_t *syst)
{
  syst->year   = ((fst >> 22 ) & 0x3FF) / 12 + 2017;
  syst->month  = ((fst >> 22 ) & 0x3FF) % 12 + 1;
  syst->day    = ((fst & 0x3FFFFF) / 86400) + 1;  // 86400=24*60*60
  syst->hour   = ((fst & 0x3FFFFF) / 3600) % 24;  // 3600=60*60
  syst->minute = ((fst & 0x3FFFFF) / 60) % 60;
  syst->second = (fst & 0x3FFFFF) % 60;
  return 0;
}

// Convert system TIME to fs-time
uint32_t fs_systime_to_fstime(time_t *syst)
{
  uint32_t fst = syst->second;
  fst += (syst->minute)*60;
  fst += (syst->hour)*60*60;
  fst += (syst->day-1)*24*60*60;
  fst &= 0x3FFFFF;
  fst |= ((syst->year-2017)*12 + (syst->month-1)) << 22;
  return fst;
}
