// Kernel

#include "types.h"
#include "kernel.h"
#include "hwio.h"
#include "ulib/ulib.h"
#include "fs.h"
#include "syscall.h"
#include "x86.h"

// Extern program call
#define UPROG_MEMLOC 0x10000L
#define UPROG_STRLOC 0x20080L
#define UPROG_ARGLOC 0x20000L

// FW declaration
uint index_to_disk(uint index);

// Handle system calls
// Usually:
// -Unpack parameters
// -Redirect to the right kernel function
// -Pack and return parameters
uint kernel_service(uint service, void* param)
{
	switch(service) {

		case SYSCALL_IO_OUT_CHAR: {
			io_vga_putc(*(char*)param);
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
			TSYSCALL_FSINFO fi;
			FS_INFO info;
			uint result;
			memcpy(&fi, param, sizeof(fi));
			result = fs_get_info(fi.disk_index, &info);
			memcpy(fi.info, &info, sizeof(info));
			return result;
		}

		case SYSCALL_FS_GET_ENTRY: {
			TSYSCALL_FSENTRY fi;
			SFS_ENTRY entry;
			FS_ENTRY o_entry;
			char path[MAX_PATH];
			uint result;
			memcpy(&fi, param, sizeof(fi));
			memcpy(path, fi.path, sizeof(path));
			result = fs_get_entry(&entry, path, fi.parent, fi.disk);
			memcpy(o_entry.name, entry.name, sizeof(o_entry.name));
			o_entry.flags = entry.flags;
			o_entry.size = entry.size;
			memcpy(fi.entry, &o_entry, sizeof(o_entry));
			return result;
		}

		case SYSCALL_FS_READ_FILE: {
			TSYSCALL_FSRWFILE fi;
			char path[MAX_PATH];
			uint offset = 0;
			memcpy(&fi, param, sizeof(fi));
			memcpy(path, fi.path, sizeof(path));
			while(offset < fi.count) {
				char tbuff[BLOCK_SIZE];
				uint count = min(sizeof(tbuff), fi.count-offset);
				uint read = fs_read_file(tbuff, path, fi.offset+offset, count);
				if(read >= ERROR_ANY) {
					offset = read;
					break;
				}
				if(read == 0) {
					break;
				}
				memcpy(fi.buff+offset, tbuff, read);
				offset += read;
			}
			return offset;
		}

		case SYSCALL_FS_WRITE_FILE: {
			TSYSCALL_FSRWFILE fi;
			char path[MAX_PATH];
			uint offset = 0;
			memcpy(&fi, param, sizeof(fi));
			memcpy(path, fi.path, sizeof(path));
			while(offset < fi.count) {
				char tbuff[BLOCK_SIZE];
				uint write;
				uint count = min(sizeof(tbuff), fi.count-offset);
				memcpy(tbuff, fi.buff+fi.offset+offset, count);

				write = fs_write_file(tbuff, path, fi.offset+offset, count, fi.flags);
				if(write >= ERROR_ANY) {
					offset = write;
					break;
				}
				offset += write;
			}
			return offset;
		}

		case SYSCALL_FS_MOVE: {
			TSYSCALL_FSSRCDST fi;
			char src[MAX_PATH];
			char dst[MAX_PATH];
			memcpy(&fi, param, sizeof(fi));
			memcpy(src, fi.src, sizeof(src));
			memcpy(dst, fi.dst, sizeof(dst));
			return fs_move(src, dst);
		}

		case SYSCALL_FS_COPY: {
			TSYSCALL_FSSRCDST fi;
			char src[MAX_PATH];
			char dst[MAX_PATH];
			memcpy(&fi, param, sizeof(fi));
			memcpy(src, fi.src, sizeof(src));
			memcpy(dst, fi.dst, sizeof(dst));
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
			TSYSCALL_FSLIST fi;
			SFS_ENTRY entry;
			FS_ENTRY o_entry;
			char path[MAX_PATH];
			uint result;
			memcpy(&fi, param, sizeof(fi));
			memcpy(path, fi.path, sizeof(path));
			result = fs_list(&entry, path, fi.n);
			memcpy(o_entry.name, entry.name, sizeof(o_entry.name));
			o_entry.flags = entry.flags;
			o_entry.size = entry.size;
			memcpy(fi.entry, &o_entry, sizeof(o_entry));
			return result;
		}

		case SYSCALL_FS_FORMAT: {
			return fs_format(*(uint*)param);
		}

		case SYSCALL_TIME_GET: {
			io_gettime(param);
			return 0;
		}
	};

	return 0;
}

// The main kernel function
static void execute(char* str);
void kernel()
{
	// Install ISR
	install_ISR();

	debug_putstr("nano32 %u.%u build %u\n",
		OS_VERSION_HI, OS_VERSION_LO, OS_BUILD_NUM);

	// Setup disk identifiers
  disk_info[0].id = 0x00; // Floppy disk 0
  strncpy(disk_info[0].name, "fd0", sizeof(disk_info[0].name));

  disk_info[1].id = 0x01; // Floppy disk 1
  strncpy(disk_info[1].name, "fd1", sizeof(disk_info[1].name));

  disk_info[2].id = 0x80; // Hard disk 0
  strncpy(disk_info[2].name, "hd0", sizeof(disk_info[2].name));

  disk_info[3].id = 0x81; // Hard disk 1
  strncpy(disk_info[3].name, "hd1", sizeof(disk_info[3].name));

  // Initialize hardware related disks info
  debug_putstr("Disk auxiliar buffer at: %x\n", disk_buff);

	for(uint i=0; i<MAX_DISK; i++) {
    uint n = index_to_disk(i);

    // Try to get info
		diskinfo_t dinfo;
    uint result = io_disk_get_info(n, &dinfo);
		disk_info[i].sectors = dinfo.sector_num;
		disk_info[i].sides = dinfo.head_num;
		disk_info[i].cylinders = dinfo.cylinder_num;

    // Use retrieved data if success
    if(result == 0) {
      disk_info[i].size = (disk_info[i].sectors *
        disk_info[i].sides * disk_info[i].cylinders) /
        (1048576L / BLOCK_SIZE);

      disk_info[i].last_access = 0;

      debug_putstr("DISK (%2x : size=%u MB sect_per_track=%d, sides=%d, cylinders=%d)\n",
        n, disk_info[i].size, disk_info[i].sectors, disk_info[i].sides,
        disk_info[i].cylinders);

    } else {
      // Failed. Do not use this disk
      disk_info[i].sectors = 0;
      disk_info[i].sides = 0;
      disk_info[i].cylinders = 0;
      disk_info[i].size = 0;
      disk_info[i].last_access = 0;
    }
  }

  // Init FS info
  fs_init_info();

  putstr("Starting...\n");

	// Print current disk
	debug_putstr("system disk: %2x\n",
		system_disk);

	// Integrated CLI
	while(1) {
		char str[72];

		// Prompt and wait command
		putstr("> ");
		getstr(str, sizeof(str));

		execute(str);
	}
}

// Execute command
#define CLI_MAX_ARG 4
static void execute(char* str)
{
  uint  argc;
  char* argv[CLI_MAX_ARG];
  char* tok = str;
  char* nexttok = tok;
	uint result = 0;

  memset(argv, 0, sizeof(argv));
  debug_putstr("in> %s\n", str);

  // Tokenize
  argc = 0;
  while(*tok && *nexttok && argc < CLI_MAX_ARG) {
    tok = strtok(tok, &nexttok, ' ');
    if(*tok) {
      argv[argc++] = tok;
    }
    tok = nexttok;
  }

  // Process command
  if(argc == 0) {
    // Empty command line, skip
  }
  // Built in commands
  else if(strcmp(argv[0], "cls") == 0) {
    // Cls command: clear the screen
    if(argc == 1) {
      putstr("not implemented\n");
    } else {
      putstr("usage: cls\n");
    }

  } else if(strcmp(argv[0], "shutdown") == 0) {
    if(argc == 1) {
      putstr("not implemented\n");
    } else {
      putstr("usage: shutdown\n");
    }

	} else if(strcmp(argv[0], "config") == 0) {
    if(argc == 1) {
      putstr("not implemented\n");
    } else {
      putstr("usage: config\n");
    }

	} else if(strcmp(argv[0], "list") == 0) {
		// List command: list directory entries

		// If not path arg provided, add and implicit ROOT_DIR_NAME
		// This way it shows system disk contents
		if(argc == 1) {
			argv[1] = ROOT_DIR_NAME;
			argc = 2;
		}

		if(argc == 2) {
			char line[64];
			SFS_ENTRY entry;

			// Get number of entries in target dir
			uint n = fs_list(&entry, argv[1], 0);
			if(n >= ERROR_ANY) {
				putstr("path not found\n");
				return;
			}

			if(n > 0) {
				putstr("\n");

				// Print one by one
				for(uint i=0; i<n; i++) {
					time_t etime;
					uint c, size;

					// Get entry
					result = fs_list(&entry, argv[1], i);
					if(result >= ERROR_ANY) {
						putstr("Error\n");
						break;
					}

					// Listed entry is a dir? If so,
					// start this line with a '+'
					memset(line, 0, sizeof(line));
					strncpy(line, entry.flags & T_DIR ? "+ " : "  ", sizeof(line));
					strncat(line, (char*)entry.name, sizeof(line)); // Append name

					// We want size to be right-aligned so add spaces
					// depending on figures of entry size
					for(c=strlen(line); c<22; c++) {
						line[c] = ' ';
					}
					size = entry.size;
					while((size = size / 10)) {
						line[--c] = 0;
					}

					// Print name and size
					putstr("%s%u %s   ", line, (uint)entry.size,
						(entry.flags & T_DIR) ? "items" : "bytes");

					// Print date
					fs_fstime_to_systime(entry.time, &etime);
					putstr("%4d/%2d/%2d %2d:%2d:%2d\n",
						etime.year,
						etime.month,
						etime.day,
						etime.hour,
						etime.minute,
						etime.second);
				}
				putstr("\n");
			}
		} else {
			putstr("usage: list <path>\n");
		}

	} else if(strcmp(argv[0], "makedir") == 0) {
    // Makedir command
    if(argc == 2) {
      result = fs_create_directory(argv[1]);
      if(result == ERROR_NOT_FOUND) {
        putstr("error: path not found\n");
      } else if(result == ERROR_EXISTS) {
        putstr("error: destination already exists\n");
      } else if(result == ERROR_NO_SPACE) {
        putstr("error: can't allocate destination in filesystem\n");
      } else if(result >= ERROR_ANY) {
        putstr("error: coludn't create directory\n");
      }
    } else {
      putstr("usage: makedir <path>\n");
    }

  } else if(strcmp(argv[0], "delete") == 0) {
    // Delete command
    if(argc == 2) {
      result = fs_delete(argv[1]);
      if(result >= ERROR_ANY) {
        putstr("error: failed to delete\n");
      }
    } else {
      putstr("usage: delete <path>\n");
    }

	} else if(strcmp(argv[0], "move") == 0) {
		// Move command
		if(argc == 3) {
			result = fs_move(argv[1], argv[2]);
			if(result == ERROR_NOT_FOUND) {
				putstr("error: path not found\n");
			} else if(result == ERROR_EXISTS) {
				putstr("error: destination already exists\n");
			} else if(result == ERROR_NO_SPACE) {
				putstr("error: can't allocate destination in filesystem\n");
			} else if(result >= ERROR_ANY) {
				putstr("error: coludn't move files\n");
			}
		} else {
			putstr("usage: move <path> <newpath>\n");
		}

	} else if(strcmp(argv[0], "copy") == 0) {
		// Copy command
		if(argc == 3) {
			result = fs_copy(argv[1], argv[2]);
			if(result == ERROR_NOT_FOUND) {
				putstr("error: path not found\n");
			} else if(result == ERROR_EXISTS) {
				putstr("error: destination already exists\n");
			} else if(result == ERROR_NO_SPACE) {
				putstr("error: can't allocate destination in filesystem\n");
			} else if(result >= ERROR_ANY) {
				putstr("error: coludn't copy files\n");
			}
		} else {
			putstr("usage: copy <srcpath> <dstpath>\n");
	  }

	} else if(strcmp(argv[0], "info") == 0) {
		// Info command: show system info
		if(argc == 1) {
			putstr("\n");
			putstr("NANO-S32 [Version %u.%u build %u]\n",
				OS_VERSION_HI, OS_VERSION_LO, OS_BUILD_NUM);
			putstr("\n");

			putstr("Disks:\n");
			fs_init_info(); // Rescan disks
			for(uint i=0; i<MAX_DISK; i++) {
				uint n = index_to_disk(i);
				if(disk_info[i].size) {
					putstr("%s %s(%uMB)   Disk size: %uMB\n",
						disk_to_string(n), disk_info[i].fstype == FS_TYPE_NSFS ? "NSFS" : "UNKN",
						blocks_to_MB(disk_info[i].fssize), disk_info[i].size);
				}
			}
			putstr("\n");
			putstr("System disk: %s\n", disk_to_string(system_disk));
			putstr("\n");
		} else {
			putstr("usage: info\n");
		}

	} else if(strcmp(argv[0], "clone") == 0) {
		// Clone command: clone system disk in another disk
		if(argc == 2) {
			SFS_ENTRY entry;
			uint disk, disk_index;
			uint sysdisk_index = disk_to_index(system_disk);

			// Show source disk info
			putstr("System disk: %s    fs=%s  size=%uMB\n",
				disk_to_string(system_disk),
				disk_info[sysdisk_index].fstype == FS_TYPE_NSFS ? "NSFS   " : "unknown",
				blocks_to_MB(disk_info[sysdisk_index].fssize));

			// Check target disk
			disk = string_to_disk(argv[1]);
			if(disk == ERROR_NOT_FOUND) {
				putstr("Target disk not found (%s)\n", argv[1]);
				return;
			}
			if(disk == system_disk) {
				putstr("Target disk can't be the system disk\n");
				return;
			}

			// Show target disk info
			disk_index = disk_to_index(disk);
			putstr("Target disk: %s    fs=%s  size=%uMB\n",
				disk_to_string(disk),
				disk_info[disk_index].fstype == FS_TYPE_NSFS ? "NSFS   " : "unknown",
				blocks_to_MB(disk_info[disk_index].fssize));

			putstr("\n");

			// User should know this
			putstr("Target disk (%s) will lose all data\n", disk_to_string(disk));
			putstr("Target disk (%s) will contain a %uMB NSFS filesystem after operation\n",
				disk_to_string(disk), disk_info[disk_index].size);

			// Ask for confirmation
			putstr("\n");
			putstr("Press 'y' to confirm: ");
			if(getkey() != 'y') {
				putstr("\nUser aborted operation\n");
				return;
			}

			putstr("y\n");

			// Format disk and copy kernel
			putstr("Formatting and copying system files...\n");
			result = fs_format(disk);
			if(result != 0) {
				putstr("Error formatting disk. Aborted\n");
				return;
			}

			// Copy user files
			putstr("Copying user files...\n");

			uint n = fs_list(&entry, ROOT_DIR_NAME, 0);
			if(n >= ERROR_ANY) {
				putstr("Error creating file list\n");
				return;
			}

			// List entries
			for(uint i=0; i<n; i++) {
				char dst[MAX_PATH];
				result = fs_list(&entry, ROOT_DIR_NAME, i);
				if(result >= ERROR_ANY) {
					putstr("Error copying files. Aborted\n");
					break;
				}

				strncpy(dst, argv[1], sizeof(dst));
				strncat(dst, PATH_SEPARATOR_S, sizeof(dst));
				strncat(dst, (char*)entry.name, sizeof(dst));

				debug_putstr("copy %s %s\n", entry.name, dst);
				result = fs_copy((char*)entry.name, dst);
				// Skip ERROR_EXISTS errors, because system files were copied
				// by fs_format function, so they are expected to fail
				if(result >= ERROR_ANY && result != ERROR_EXISTS) {
					putstr("Error copying %s. Aborted\n", entry.name);
					break;
				}
			}

			// Notify result
			if(result < ERROR_ANY) {
				putstr("Operation completed\n");
			}
		} else {
			putstr("usage: clone <target_disk>\n");
		}

	} else if(strcmp(argv[0], "read") == 0) {
		// Read command: read a file
		if(argc==2 || (argc==3 && strcmp(argv[1],"hex")==0)) {
			uint offset = 0;
			char buff[512];
			memset(buff, 0, sizeof(buff));
			// While it can read the file, print it
			while((result = fs_read_file(buff, argv[argc-1], offset, sizeof(buff)))) {
				if(result >= ERROR_ANY) {
					putstr("\nThere was an error reading input file\n");
					break;
				}
				for(uint i=0; i<result; i++) {
					if(argc==2) {
						putc(buff[i]);
					} else {
						putstr("%2x ", buff[i]);
					}
				}
				memset(buff, 0, sizeof(buff));
				offset += result;
			}
			putstr("\n");
		} else {
			putstr("usage: read [hex] <path>\n");
		}

	} else if(strcmp(argv[0], "time") == 0) {
		// Time command: Show date and time
		if(argc == 1) {
			time_t ctime;
			time(&ctime);

			putstr("\n%4d/%2d/%2d 2%d:%2d:%2d\n\n",
				ctime.year,
				ctime.month,
				ctime.day,
				ctime.hour,
				ctime.minute,
				ctime.second);
		} else {
			putstr("usage: time\n");
		}

	} else if(strcmp(argv[0], "help") == 0) {
		// Help command: Show help, and some tricks
		if(argc == 1) {
			putstr("\n");
			putstr("Built-in commands:\n");
			putstr("\n");
			putstr("clone    - clone system in another disk\n");
			putstr("cls      - clear the screen\n");
			putstr("config   - show or set config\n");
			putstr("copy     - create a copy of a file or directory\n");
			putstr("delete   - delete entry\n");
			putstr("help     - show this help\n");
			putstr("info     - show system info\n");
			putstr("list     - list directory contents\n");
			putstr("makedir  - create directory\n");
			putstr("move     - move file or directory\n");
			putstr("read     - show file contents in screen\n");
			putstr("shutdown - shutdown the computer\n");
			putstr("time     - show time and date\n");
			putstr("\n");
		} else if(argc == 2 &&  // Easter egg
			(strcmp(argv[1], "huri") == 0 || strcmp(argv[1], "marylin") == 0)) {
			putstr("\n");
			putstr("                                     _,-/\\^---,      \n");
			putstr("             ;\"~~~~~~~~\";          _/;; ~~  {0 `---v \n");
			putstr("           ;\" :::::   :: \"\\_     _/   ;;     ~ _../  \n");
			putstr("         ;\" ;;    ;;;       \\___/::    ;;,'~~~~      \n");
			putstr("       ;\"  ;;;;.    ;;     ;;;    ::   ,/            \n");
			putstr("      / ;;   ;;;______;;;;  ;;;    ::,/              \n");
			putstr("     /;;V_;; _-~~~~~~~~~~;_  ;;;   ,/                \n");
			putstr("    | :/ / ,/              \\_  ~~)/                  \n");
			putstr("    |:| / /~~~=              \\;; \\~~=                \n");
			putstr("    ;:;{::~~~~~~=              \\__~~~=               \n");
			putstr(" ;~~:;  ~~~~~~~~~               ~~~~~~               \n");
			putstr(" \\/~~                                               \n");
			putstr("\n");
		} else {
			putstr("usage: help\n");
		}

	} else {
		// Not a built-in command
		// Try to find an executable file
		char* prog_ext = ".bin";
		SFS_ENTRY entry;
		char prog_file_name[32];
		strncpy(prog_file_name, argv[0], sizeof(prog_file_name));

		// Append .bin if there is not a '.' in the name
		if(!strchr(prog_file_name, '.')) {
			strncat(prog_file_name, prog_ext, sizeof(prog_file_name));
		}

		// Find .bin file
		result = fs_get_entry(&entry, prog_file_name, UNKNOWN_VALUE, UNKNOWN_VALUE);
		if(result < ERROR_ANY) {
			// Found
			if(entry.flags & T_FILE) {
				uint offset = 0;
				// It's a file: load it
				uint mem_size = min((uint)entry.size, UPROG_ARGLOC-UPROG_MEMLOC);
				if(mem_size < (uint)entry.size) {
					putstr("not enough memory\n");
					return;
				}

				while(offset < mem_size) {
					uint r;
					uint count;
					char buff[512];
					count = min(mem_size-offset, sizeof(buff));
					r = fs_read_file(buff, prog_file_name, offset, count);
					if(r<ERROR_ANY) {
						uint8_t* ptr = (uint8_t*)UPROG_MEMLOC;
						for(uint b=0; b<r; b++) {
							ptr[offset+b] = buff[b];
						}
						offset += r;
					} else  {
						putstr("error loading file\n");
						debug_putstr("error loading file\n");
						result = ERROR_IO;
						break;
					}
				}
			} else {
				// It's not a file: error
				result = ERROR_NOT_FOUND;
			}
		}

		if(result >= ERROR_ANY || result == 0) {
			putstr("unkown command\n");
		} else {
			uint uarg = 0;
			uint c = 0;
			char** arg_var = (char**)UPROG_ARGLOC;
			char* arg_str = (char*)UPROG_STRLOC;

			// Check name ends with ".bin"
			if(strcmp(&prog_file_name[strchr(prog_file_name, '.') - 1], prog_ext)) {
				putstr("error: only %s files can be executed\n", prog_ext);
				return;
			}

			// Create argv copy in program segment
			for(uarg=0; uarg<argc; uarg++) {
				arg_var[uarg] = (char*)(UPROG_STRLOC+c);
				for(uint i=0; i<strlen(argv[uarg])+1; i++) {
					arg_str[c] = argv[uarg][i];
					c++;
				}
			}

			debug_putstr("CLI: Running program %s (%d bytes)\n",
				prog_file_name, (uint)entry.size);

			int (*uprog)(int, void*) = (void*)UPROG_MEMLOC;

			// Run program
			uprog(argc, (void*)UPROG_ARGLOC);
		}
	}
}
