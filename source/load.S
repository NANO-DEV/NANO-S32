# Description: This file contains the kernel startup
# Start the CPU: switch to 32-bit protected mode and
# call kernel c function
# Some parts are based in xv6 source code

# The primary boot loader code supplies the following parameters:
# dl = Boot-disk

# Serial port is also initialized here

# Macros to create x86 segments

#define SEG_NULLASM                                             \
        .word 0, 0;                                             \
        .byte 0, 0, 0, 0

# The 0xC0 means the limit is in 4096-byte units
# and (for executable segments) 32-bit mode
#define SEG_ASM(bits,type,base,lim)                                  \
        .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);      \
        .byte (((base) >> 16) & 0xff), (0x90 | (type)),         \
              (bits | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#define SBITS_16  0x00
#define SBITS_32  0xC0

#define STA_X     0x8          // Executable segment
#define STA_E     0x4          // Expand down (non-executable segments)
#define STA_C     0x4          // Conforming code segment (executable only)
#define STA_W     0x2          // Writeable (non-executable segments)
#define STA_R     0x2          // Readable (executable segments)
#define STA_A     0x1          // Accessed

#define CR0_PE    0x00000001   // Protection Enable

#define SEG_KCODE 1            // kernel code
#define SEG_KDATA 2            // kernel data+stack


# Loader starts here

.code16                        # Assemble for 16-bit mode
.globl start, system_hwdisk, serial_status
start:
  cli                         # BIOS enabled interrupts; disable

  # Zero data segment registers DS, ES, and SS
  xorw    %ax,%ax              # Set %ax to zero
  movw    %ax,%ds              # -> Data Segment
  movw    %ax,%es              # -> Extra Segment
  movw    %ax,%ss              # -> Stack Segment

  movb    %dl, system_hwdisk  # save boot disk as system disk

  # Init serial port
  movw    $0, %dx
  movb    $0, %ah
  movb    $0b10101011, %al     # 2400 baud, 8 data bits, odd parity, 1 stop bit
  int     $0x14
  movb    %ah, serial_status

  # Enable physical address line A20
seta20.1:
  inb     $0x64,%al            # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.1

  movb    $0xd1,%al            # 0xd1 -> port 0x64
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al            # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al            # 0xdf -> port 0x60
  outb    %al,$0x60

  # Switch from real to protected mode.  Use a flat GDT that makes
  # virtual addresses map directly to physical addresses so that the
  # effective memory map doesn't change during the transition
  lgdt    gdtdesc  # gdt16_ptr
  movl    %cr0, %eax
  orl     $CR0_PE, %eax
  movl    %eax, %cr0

  # Complete the transition to 32-bit protected mode by using a long jmp
  # to reload %cs and %eip.  The segment descriptors are set up with no
  # translation, so that the mapping is still the identity mapping
  ljmp    $(SEG_KCODE<<3), $start32

.code32  # Tell assembler to generate 32-bit code now
start32:
  # Set up the protected-mode data segment registers
  movw    $(SEG_KDATA<<3), %ax # Data segment selector
  movw    %ax, %ds             # -> DS: Data Segment
  movw    %ax, %es             # -> ES: Extra Segment
  movw    %ax, %ss             # -> SS: Stack Segment
  movw    $0,  %ax             # Zero segments not ready for use
  movw    %ax, %fs             # -> FS
  movw    %ax, %gs             # -> GS

  # Set up the stack pointer and call C function
  cld
  movl    $start, %esp
  call    kernel

  # If bootmain returns (it shouldn't), loop
spin:
  jmp     spin

system_hwdisk:
  .byte 0
serial_status:
  .byte 0

# Bootstrap GDT
 .p2align 2                                # force 4 byte alignment
 gdt:
   SEG_NULLASM                             # null seg
   SEG_ASM(SBITS_32,STA_X|STA_R, 0x0, 0xffffffff)   # code seg
   SEG_ASM(SBITS_32,STA_W, 0x0, 0xffffffff)         # data seg
   SEG_ASM(SBITS_16,STA_X|STA_R, 0x0, 0xffffffff)   # code seg
   SEG_ASM(SBITS_16,STA_W, 0x0, 0xffffffff)         # data seg

 gdtdesc:
   .word   (gdtdesc - gdt - 1)             # sizeof(gdt) - 1
   .long   gdt                             # address gdt
