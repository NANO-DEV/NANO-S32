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
#define SEG_ASM(bits,type,base,lim)                             \
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
.globl start, system_hwdisk
start:
  cli                         # BIOS enabled interrupts; disable

  # Zero data segment registers DS, ES, and SS
  xorw    %ax,%ax              # Set %ax to zero
  movw    %ax,%ds              # -> Data Segment
  movw    %ax,%es              # -> Extra Segment
  movw    %ax,%ss              # -> Stack Segment

  movb    %dl,system_hwdisk    # save boot disk as system disk

  movw    $0x1111,%ax          # Set font
  movb    $0, %bl
  int     $0x10

  # Enable A20 line
  movw    $0x2401,%ax          # A20-Gate Activate by BIOS
  int     $0x15

;  movb $0xFF,%al
;  outb %al,$0xA1
;  outb %al,$0x21

  # Switch from real to protected mode.  Use a flat GDT that makes
  # virtual addresses map directly to physical addresses so that the
  # effective memory map doesn't change during the transition
  lgdt    gdtdesc
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
  movl    $start, %esp
  call    kernel

  # If bootmain returns (it shouldn't), loop
spin:
  jmp     spin

system_hwdisk:
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
