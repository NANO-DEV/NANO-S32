// Integrated CLI
// with built-in commands

#include "types.h"
#include "kernel.h"
#include "hwio.h"
#include "ulib/ulib.h"
#include "fs.h"
#include "x86.h"
#include "cli.h"

// Extern program call
#define UPROG_MEMLOC 0x20000U
#define UPROG_MEMMAX 0x10000U
#define UPROG_ARGLOC 0x18000U
#define UPROG_STRLOC 0x18080U

// Built-in commands implementation

// cls command: clear the screen
static void cli_cls(uint argc)
{
  if(argc == 1) {
    clear_screen();
  } else {
    putstr("usage: cls\n");
  }
}

// Shutdown computer
static void cli_shutdown(uint argc)
{
  if(argc == 1) {
    putstr("Shutting down...\n\n");
    io_vga_clear();
    io_vga_showcursor(0);
    putstr("Turn off computer");
    apm_shutdown();
  } else {
    putstr("usage: shutdown\n");
  }
}

// list directory entries
static void cli_list(uint argc, char* argv[])
{
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
        uint result = 0;
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
        putstr("%4u/%2u/%2u %2u:%2u:%2u\n",
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
}

// Create directory
static void cli_makedir(uint argc, char* argv[])
{
  if(argc == 2) {
    uint result = 0;
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
}

static void cli_delete(uint argc, char* argv[])
{
  if(argc == 2) {
    uint result = 0;
    result = fs_delete(argv[1]);
    if(result >= ERROR_ANY) {
      putstr("error: failed to delete\n");
    }
  } else {
    putstr("usage: delete <path>\n");
  }
}

// Move/rename
static void cli_move(uint argc, char* argv[])
{
  if(argc == 3) {
    uint result = 0;
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
}

static void cli_copy(uint argc, char* argv[])
{
  if(argc == 3) {
    uint result = 0;
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
}

// Show system info
static void cli_info(uint argc)
{
  if(argc == 1) {
    putstr("\n");
    putstr("NANO-S32 [Version %u.%u build %u]\n",
      OS_VERSION_HI, OS_VERSION_LO, OS_BUILD_NUM);
    putstr("\n");
    putstr("Disks:\n");
    fs_init_info(); // Rescan disks
    for(uint i=0; i<MAX_DISK; i++) {
      if(disk_info[i].size) {
        putstr("%s %s(%uMB)   Disk size: %uMB   %s\n",
          disk_to_string(i), disk_info[i].fstype == FS_TYPE_NSFS ? "NSFS" : "UNKN",
          blocks_to_MB(disk_info[i].fssize), disk_info[i].size, disk_info[i].desc);
      }
    }
    putstr("\n");
    putstr("System disk: %s\n", disk_to_string(system_disk));
    putstr("\n");
    dump_regs();
  } else {
    putstr("usage: info\n");
  }
}

// Clone system disk in another disk
static void cli_clone(uint argc, char* argv[])
{
  if(argc == 2) {
    SFS_ENTRY entry;
    uint disk;
    uint result = 0;
    // Show source disk info
    putstr("System disk: %s    fs=%s  size=%uMB\n",
      disk_to_string(system_disk),
      disk_info[system_disk].fstype == FS_TYPE_NSFS ? "NSFS   " : "unknown",
      disk_info[system_disk].fstype == FS_TYPE_NSFS ?
      blocks_to_MB(disk_info[system_disk].fssize) :
      disk_info[system_disk].size );
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
    putstr("Target disk: %s    fs=%s  size=%uMB\n",
      disk_to_string(disk),
      disk_info[disk].fstype == FS_TYPE_NSFS ? "NSFS   " : "unknown",
      disk_info[disk].fstype == FS_TYPE_NSFS ?
      blocks_to_MB(disk_info[disk].fssize) :
      disk_info[disk].size );
    putstr("\n");
    // User should know this
    putstr("Target disk (%s) will lose all data\n", disk_to_string(disk));
    putstr("Target disk (%s) will contain a %uMB NSFS filesystem after operation\n",
      disk_to_string(disk), disk_info[disk].size);
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
}

// Read (display) a file
static void cli_read(uint argc, char* argv[])
{
  if(argc==2 || (argc==3 && strcmp(argv[1],"hex")==0)) {
    uint result = 0;
    uint offset = 0;
    char buff[512];
    memset(buff, 0, sizeof(buff));
    // While it can read the file, print it
    while((result = fs_read_file(buff, argv[argc-1], offset, sizeof(buff)))) {
      if(result == ERROR_NOT_FOUND) {
        putstr("\nInvalid input file\n");
        break;
      } else if(result >= ERROR_ANY) {
        putstr("\nThere was an error reading input file\n");
        break;
      }
      for(uint i=0; i<result; i++) {
        if(argc==2) {
          putc(buff[i]);
        } else {
          uint uc = (uint8_t)buff[i];
          putstr("%2x ", uc);
        }
      }
      memset(buff, 0, sizeof(buff));
      offset += result;
    }
    putstr("\n");
  } else {
    putstr("usage: read [hex] <path>\n");
  }
}

// Show date and time
static void cli_time(uint argc)
{
  if(argc == 1) {
    time_t ctime;
    get_datetime(&ctime);
    putstr("\n%4u/%2u/%2u %2u:%2u:%2u\n\n",
      ctime.year,
      ctime.month,
      ctime.day,
      ctime.hour,
      ctime.minute,
      ctime.second);
  } else {
    putstr("usage: time\n");
  }
}

// Not a built-in command case:
// -Try to find and run executable file
// -Show "Unknown command" error if not found
static void cli_extern(uint argc, char* argv[])
{
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
  uint result = 0;
  result = fs_get_entry(&entry, prog_file_name, UNKNOWN_VALUE, UNKNOWN_VALUE);
  if(result < ERROR_ANY) {
    // Found
    if(entry.flags & T_FILE) {
      // It's a file: load it
      if(UPROG_MEMMAX < entry.size) {
        putstr("not enough memory\n");
        return;
      }
      uint r = fs_read_file((void*)UPROG_MEMLOC, prog_file_name, 0, entry.size);
      if(r>=ERROR_ANY) {
        putstr("error loading file\n");
        debug_putstr("error loading file\n");
        result = ERROR_IO;
        return;
      }
    } else {
      // It's not a file: error
      result = ERROR_NOT_FOUND;
    }
  }

  if(result >= ERROR_ANY || result == 0) {
    putstr("unknown command\n");
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

    debug_putstr("CLI: Running program %s (%u bytes)\n",
      prog_file_name, entry.size);

    int (*user_prog)(int, void*) = (void*)UPROG_MEMLOC;

    // Run program
    user_prog(argc, (void*)UPROG_ARGLOC);
  }
}



// Command line interface

// Execute a command
#define CLI_MAX_ARG 4
static void execute(char* str)
{
  uint  argc;
  char* argv[CLI_MAX_ARG];
  char* tok = str;
  char* nexttok = tok;

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
  // Built-in commands
  else if(strcmp(argv[0], "cls") == 0) {
    // cls command: clear the screen
    cli_cls(argc);

  } else if(strcmp(argv[0], "shutdown") == 0) {
    cli_shutdown(argc);

  } else if(strcmp(argv[0], "list") == 0) {
    // List files and dirs
    cli_list(argc, argv);

  } else if(strcmp(argv[0], "makedir") == 0) {
    cli_makedir(argc, argv);

  } else if(strcmp(argv[0], "delete") == 0) {
    cli_delete(argc, argv);

  } else if(strcmp(argv[0], "move") == 0) {
    // Move/rename files and dirs
    cli_move(argc, argv);

  } else if(strcmp(argv[0], "copy") == 0) {
    cli_copy(argc, argv);

  } else if(strcmp(argv[0], "info") == 0) {
    // Show system info
    cli_info(argc);

  } else if(strcmp(argv[0], "clone") == 0) {
    // Clone running system in a disk
    cli_clone(argc, argv);

  } else if(strcmp(argv[0], "read") == 0) {
    // Read (display) file contents
    cli_read(argc, argv);

  } else if(strcmp(argv[0], "time") == 0) {
    // Show time and date
    cli_time(argc);

  } else if(strcmp(argv[0], "help") == 0) {
    // Show help
    if(argc == 1) {
      putstr("\n");
      putstr("Built-in commands:\n");
      putstr("\n");
      putstr("clone    - clone system in another disk\n");
      putstr("cls      - clear the screen\n");
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
    } else if(argc == 2 && strcmp(argv[1], "huri") == 0) {
      // Easter egg
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
    // Try to find and run executable file
    // Show "Unknown command" error if not found
    cli_extern(argc, argv);
  }
}

// Command line interface - main loop
void cli()
{
  while(1) {
    char str[72];

    // Prompt and wait command
    putstr("> ");
    getstr(str, sizeof(str));

    // Execute
    execute(str);
  }
}
