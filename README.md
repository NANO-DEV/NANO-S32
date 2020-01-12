# NANO S32

NANO S32 is a small operating system for 32-bit x86 architectures which operates in protected mode.

System requirements:
* x86 compatible computer
* 1MB of RAM
* 1.44Mb disk
* VGA graphics card

Realtek 8029AS network card is supported.

Developer notes:
This software is a hobby operating system. Creator makes no warranty for its use and assumes no responsibility for any errors which may appear in this document.

## System description
This operating system is composed of several components, detailed in this section.

### The kernel
The kernel is the core of the operating system. It connects the application software to the hardware of a computer. The kernel manages memory access for user programs, determines how programs get access to hardware resources, and organizes the data for long-term non-volatile storage with file systems on disks. This operating system kernel is written in C and assembler.

#### Program execution
The operating system provides an interface between an application program and the computer hardware, so that an application program can interact with the hardware through procedures programmed into the operating system. To do so, the operating system provides a set of services which simplify development and execution of application programs. 

To execute an application program the operating system assigns memory space and other resources, loads program binary code into memory, and initiates execution of the application program which then interacts with the user and with hardware devices.

This operating system implements a monotasking task model: only one thread of execution is run at a given time. When an application is executed, it takes control of the whole computer, save for the 'resident' part of the operating system which handles system calls and hardware interrupts.

#### Operation Mode
This operating system operates kernel and applications in Protected Mode, although some of its endowed hardware controllers perform temporally switches to 16-bit Real Mode to access BIOS services.

#### Memory management
The operating system kernel is responsible for managing the system memory. This operating system enables A20 line (if available) at startup and uses a flat memory model. 4GB of memory are addressable. A heap is managed by the kernel to allow user memory allocation.

#### Disk access and file systems
The specific way in which files are stored on a disk is called a file system. File systems allow users and programs to organize files on a computer.

This operating system supports hard disks and removable disks, and implements a custom file system (NSFS). The NSFS file system is simple and easy to implement.

NSFS divides disk space into logical blocks of contiguous space, following this layout:

[boot block | super block | entries table | data blocks]
* Boot block (block 0): Boot sector
* Super block (block 1): Contains information about the layout of the file system
* Entries table (blocks 2-n): Table of file and directory entries
* Data blocks (blocks n-end): Data blocks referenced by file entries

### User Interface
This operating system implements a command-line interface (CLI), where computer commands are typed out line-by-line. The User Manual section contains a more detailed description.

### Boot process
When a computer is switched on or reset, it runs through a series of diagnostics called POST (Power-On Self-Test). This sequence culminates in locating a bootable device, such as a removable disk or a hard disk in the order that the firmware is configured to. A device is detected as bootable if it carries a boot sector (512 bytes) with the byte sequence 0x55, 0xAA in bytes 511 and 512 respectively. When the BIOS finds such a boot sector, it loads it into memory at 0x0000:0x7C00 (segment 0, offset 0x7C00). Execution is then transferred to this address.

The NANO system disk contains a bootloader in this sector. This bootloader scans then the boot disk and loads the kernel file at memory location 0x8000. Once the bootloader has loaded the kernel, it jumps to this memory location to begin executing it.

The kernel execution starts, sets up needed drivers and initializes the Command Line Interface.

### Executable format
NANO executable file format does not contain metadata or sections. It only contains non-relocatable raw x86 machine code assumed to be loaded at 0x20000 address. Execution starts at this address through a call instruction. The CLI requires a file name to contain the `.bin` suffix before starting the execution of such file.

## Building
The building process is expected to be executed in a Linux system. In Windows 10 it can be built using Windows Subsystem for Linux.

1. Install required software:
    * make, gcc, nasm to build the operating system and user programs
    * optionally install qemu x86 emulator to test the images in a virtual machine
    * optionally install exuberant-ctags to generate tags
    * optionally use dd to write disk images on storage devices
    ```
    sudo apt-get install gcc nasm make
    sudo apt-get install qemu qemu-system-x86
    ```

2. Get full source code tree.
```
(sudo apt-get install git)
git clone https://github.com/NANO-DEV/NANO-S32.git
```
The tree contains the following directories:
  * fstools: disk image generation tool
  * images: output folder for generated disk images
  * source: source code
      * boot: code for the boot sector image
      * ulib: library to develop user programs
      * programs: user programs

3. Build: Run `make` from the root directory to build everything. Images will be generated in the `images` directory. Optionally, `Makefile` and `source/Makefile` files can be customized.

## Testing
After building, run `make qemu` (linux) or `qemu.bat` (windows) from the root directory to test the operating system in qemu. Other virtual machines have been successfully tested. To test the system using VirtualBox, create a new `Other/DOS` machine and start it with `images/os_fd.img` as floppy image.

The network support has been only tested in qemu under Windows, using the Tap-windows driver provided [here](https://openvpn.net/index.php/download/community-downloads.html). This virtual device must be renamed to `tap` and bridged to the actual nic in order to make the default `qemu.bat` script work as expected.

The operating system outputs debug information through the first serial port in real time. This can be useful for developers. This serial port is configured to work at 2400 bauds, 8 data bits, odd parity and 1 stop bit.

Using the provided qemu scripts, the serial port is automatically mapped to the process standard input/output. For VirtualBox users, it is possible for example to telnet from putty if COM1 is set to TCP mode without a pipe, and the same port is specified in both programs.

The system can operate real hardware if images are written to physical disks. Writing images to disks to test them in real hardware is dangerous and can cause loss of data or boot damage, among other undesired things, in all involved computers. So, it is not recommended to proceed unless this risk is understood and assumed. To write images to physical disks, `dd` can be used in linux:
```
dd if=images/os-fd.img of=/dev/sdb status=none
```
Replace `/dev/sdb` with the actual target storage device. Usually `root` privileges will be needed to run this command.

## User manual
### First boot
A system disk is needed to start the operating system for first time. This disk contains special files required for the computer to boot. If the system disk is removable media like a floppy diskette, it must be introduced in the computer before the boot sequence starts (for example, before turning the computer on). Check also the settings in the BIOS to ensure proper boot device configuration.

See later sections for information about how to install (clone system) once it is running.

### Command Line Interface
Once the computer is turned on, and after boot sequence, the operating system automatically starts the Command Line Interface (CLI). The prompt shows an open angle bracket `>` symbol and a blinking cursor (underscore) where a command is expected to be introduced using the keyboard.

A command is a short text string constituted of several parts. The first part, is the command name (just a word), which allows the system to identify which command must be executed. Sometimes, after the name, one or more parameters must be provided, depending on the specific syntax of each command. For example, `copy doc.txt doc-new.txt` is a command made of three parts: the name (`copy`) and two parameters (`doc.txt` and `doc-new.txt`), whose meaning is specific to the copy command syntax. System commands syntax is discussed in a later section.

There are two different types of valid commands:
* The path of an executable file (`.bin`) will cause the system to execute it.
* There are also some built-in commands in the CLI (see later section).

The `.bin` suffix can be omitted for executable files. For example, to run `edit.bin`, it's enough to write `edit`. After typing a command, `ENTER` must be pressed in order to execute it.

### File system and paths
Information is stored on disks using a files and directories analogy, called file system. Directories perform a classifying function, and can contain other directories and files. Files are the actual data holders, and can only contain information, such as text, images or even executable code. A disk contains at least a top level directory, usually referred to as root directory. This term comes from an analogy where a file system is a tree, the directories are its branches and the files are its leaves.

Since most commands perform files or directories operations, a way to identify which files or directories to operate is needed. This is achieved through a specific type of structured text strings that unequivocally refer a file or directory, named paths. Paths are usually used as command parameters.

Paths can be specified as absolute paths or relative to the system disk. When specified as absolute, they must begin with a disk identifier. Possible disk identifiers are:
* fd0 - First floppy disk
* fd1 - Second floppy disk
* hd0 - First hard disk
* hd1 - Second hard disk

After the optional disk identifier, paths are formed of a sequence of components. Each component, represents a branch in the tree (a directory name), describing the full path from the root to a given branch or leave. Path components are separated with slashes `/`. The root directory of a disk can be omitted or referred as `.`.

Examples of valid paths:
```
fd0
hd0/.
hd0/documents/file.txt
docs/other/readme.txt
```

Note that all files and directories have their own name. This name must not contain spaces or slashes, and must be unique (differentiation is case sensitive) inside its parent directory.

When booting the operating system from a flash drive, the BIOS emulates instead a floppy disk or a hard disk, so the flash drive can still be accessed using one of the previous identifiers.

### CLI Built-in commands

The CLI provides some built-in commands, detailed in this section. When executed with wrong syntax, a syntax reminder is printed to screen.

#### CLONE
Clone the system disk in another disk. The target disk, after being formatted, will be able to boot and will contain a copy of all the files in the current system disk. Any previously existing data in the target disk will be lost. One parameter is expected: the target disk identifier.

Example:
```
clone hd0
```

#### CLS
Clear the screen.

#### CONFIG
Show, set or save configuration parameter values. To show current values, call it without arguments. To set values, two parameters are expected: the parameter name to set, and its value. To save configuration to file, use `save` parameter. Saved configuration will be automatically read when the system starts.

Example:
```
config
config save
config net_IP 192.168.0.20
config net_gate 192.168.0.1
```

#### COPY
Copy files. Two parameters are expected: the path of the file to copy, and the path of the new copy.

Example:
```
copy doc.txt doc-copy.txt
```

#### DELETE
Delete a file or a directory. One parameter is expected: the path of the file or directory to delete.

Example:
```
delete doc.txt
```

#### HELP
Show basic help.

#### INFO
Show system version and hardware information.

#### LIST
List the contents of a directory. One parameter is expected: the path of the directory to list. If this parameter is omitted, the contents of the system disk root directory will be listed.

Example:
```
list fd0/documents
```

#### MAKEDIR
Create a new directory. One parameter is expected: the path of the new directory.

Example:
```
makedir documents/newdir
```

#### MOVE
Move files. Two parameters are expected: the current path of the file to move, and the new path.

Example:
```
move fd0/doc.txt hd0/documents/doc.txt
```

#### READ
Display the contents of a file. The path of the file to display is expected as only parameter. Optionally, if `hex`  is passed as first parameter, contents are dumped in hexadecimal instead of ASCII.

Example:
```
read documents/doc.txt
read hex sample.bin
```

#### SHUTDOWN
Shutdowns the computer or halts it if APM is not supported.

#### TIME
Show current date and time.

## User programs development

The easiest way develop new user programs is thorugh corss development:

1. Setup the development system so it contains a copy of the full operating system source tree and it's able to build and test it. See the building and testing sections in this document.

2. Create a new `programname.c` file in `source/programs` folder with this code:
    ```
    // User program: programname

    #include "types.h"
    #include "ulib/ulib.h"

    int main(int argc, char* argv[])
    {
      return 0;
    }
    ```
3. Edit this line of `source/Makefile` and add `$(PROGDIR)programname.bin` at the end, like this:

    ```
    programs: $(PROGDIR)edit.bin $(PROGDIR)programname.bin
    ```
4. Edit this line of `Makefile` and add `$(SOURCEDIR)programs/programname.bin` at the end, like this:

    ```
    USERFILES := $(SOURCEDIR)programs/edit.bin $(SOURCEDIR)programs/programname.bin
    ```
5. Add code to `programname.c`. See all available system functions in `ulib/ulib.h`
6. Build and test.
