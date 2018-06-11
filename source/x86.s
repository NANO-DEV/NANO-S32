; Several x86 utils

[bits 32]

section .text

; void dump_regs()
; Dump register values through debug output
global dump_regs
dump_regs:
  pushad

  push cs
  push ds
  push es
  push ss

  push .dumpstr
  call debug_putstr
  pop  eax

  pop  eax
  pop  eax
  pop  eax
  pop  eax

  popad
  ret

.dumpstr db "REG DUMP:",10,"SS=%x ES=%x DS=%x CS=%x",10,"DI=%x SI=%x BP=%x SP=%x",10,"BX=%x DX=%x CX=%x AX=%x",10,0
extern debug_putstr

struc regs16_t
  .di	resw 1
  .si	resw 1
  .bp	resw 1
  .sp resw 1
  .bx	resw 1
  .dx	resw 1
  .cx	resw 1
  .ax	resw 1
  .gs	resw 1
  .fs	resw 1
  .es	resw 1
  .ds	resw 1
  .ef resw 1
endstruc

k16_stack:
  times regs16_t_size db 0              ; kernel16 stack
k16_stack_top:

%define GDTENTRY(x)                     ((x) << 3)
%define CODE32                          GDTENTRY(1)	; 0x08
%define DATA32                          GDTENTRY(2)	; 0x10
%define CODE16                          GDTENTRY(3)	; 0x18
%define DATA16                          GDTENTRY(4)	; 0x20

; int32 call makes its POSSIBLE to execute BIOS interrupts
; by temporally switching to real mode
;
; Adapted from original work by Napalm (thank you!)
; License: http://creativecommons.org/licenses/by-sa/2.0/uk/
;
; Notes: int32() resets all selectors
;	void _cdelc int32(uint8_t intnum, regs16_t* regs);
;

extern lapic_inhibit, lapic_deinhibit

global int32
int32: use32
  cli
  pushad                                 ; save register state to 32bit stack
  call lapic_inhibit                     ; inhibit LAPIC interrupts
  cld                                    ; clear direction flag (copy forward)
  mov  [stack32_ptr], esp                ; save 32bit stack pointer
  lea  esi, [esp+0x24]                   ; set position of intnum on 32bit stack
  lodsd                                  ; read intnum into eax
  mov  [ib], al                          ; set interrupt immediate byte from arguments
  mov  esi, [esi]                        ; read regs pointer in esi as source
  mov  edi, k16_stack                    ; set destination to 16bit stack
  mov  ecx, regs16_t_size                ; set copy size to struct size
  mov  esp, edi                          ; save destination to 16bit stack offset
  rep  movsb                             ; copy 32bit stack to 16bit stack
  jmp  word CODE16:p_mode16              ; switch to 16bit protected mode
p_mode16: use16
  mov  ax, DATA16                        ; get 16bit data selector
  mov  ds, ax                            ; set ds to 16bit selector
  mov  es, ax                            ; set es to 16bit selector
  mov  fs, ax                            ; set fs to 16bit selector
  mov  gs, ax                            ; set gs to 16bit selector
  mov  ss, ax                            ; set ss to 16bit selector
  mov  eax, cr0                          ; get cr0 so it can be modified
  and  al,  ~0x01                        ; mask off PE bit to turn off protected mode
  mov  cr0, eax                          ; set cr0 to result
  jmp  word 0x0000:r_mode16              ; set cs:ip to enter real-mode
r_mode16: use16
  mov  ax, 0                             ; set ax to zero
  mov  ds, ax                            ; set ds to access idt16
  mov  ss, ax                            ; set ss so valid stack
  lidt [idt16_ptr]                       ; load 16bit idt
  popa                                   ; load general purpose registers from 16bit stack
  pop  gs                                ; load gs from 16bit stack
  pop  fs                                ; load fs from 16bit stack
  pop  es                                ; load es from 16bit stack
  pop  ds                                ; load ds from 16bit stack
  mov  sp, [stack32_ptr]                 ; set usable sp
  push ax
  mov  al, 0x00                          ; unmask PIC interrupts
  out  0x21, al
  out  0xA1, al
  pop  ax
  sti
  db 0xCD                                ; opcode of INT instruction with immediate byte
ib: db 0x00
  cli
  push ax
  mov  al, 0xFF                          ; mask PIC interrupts
  out  0x21, al
  out  0xA1, al
  pop  ax
  mov  sp, 0                             ; zero sp so it can be reused
  mov  ss, sp                            ; set ss so the stack is valid
  mov  sp, k16_stack_top                 ; set correct stack position to copy
  pushf                                  ; save eflags to 16bit stack
  push ds                                ; save ds to 16bit stack
  push es                                ; save es to 16bit stack
  push fs                                ; save fs to 16bit stack
  push gs                                ; save gs to 16bit stack
  pusha                                  ; save general purpose registers to 16bit stack
  mov  sp, [stack32_ptr]                 ; set usable sp
  mov  eax, cr0                          ; get cr0 so it can be modified
  or   al, 0x01                          ; set PE bit to turn on protected mode
  mov  cr0, eax                          ; set cr0 to result
  jmp  dword CODE32:p_mode32             ; switch to 32bit selector (32bit protected mode)
p_mode32: use32
  mov  ax, DATA32                        ; get 32bit data selector
  mov  ds, ax                            ; reset ds selector
  mov  es, ax                            ; reset es selector
  mov  fs, ax                            ; reset fs selector
  mov  gs, ax                            ; reset gs selector
  mov  ss, ax                            ; reset ss selector
  lidt [idtr]                            ; restore 32bit idt pointer
  mov  esp, [stack32_ptr]                ; restore 32bit stack pointer
  mov  esi, k16_stack                    ; set copy source to 16bit stack
  lea  edi, [esp+0x28]                   ; set position of regs pointer on 32bit stack
  mov  edi, [edi]                        ; use regs pointer in edi as copy destination
  mov  ecx, regs16_t_size                ; set copy size to struct size
  cld                                    ; clear direction flag (copy forward)
  rep  movsb                             ; copy (16bit stack to 32bit stack)
  call lapic_deinhibit                   ; dehinibit LAPIC interrupts
  popad                                  ; restore registers
  sti
  ret                                    ; return to caller


stack32_ptr:                             ; address in 32bit stack after
  dd 0x00000000                          ; saving all general purpose registers

idt16_ptr:                               ; IDT table pointer for 16bit access
  dw 0x03FF                              ; table limit (size)
  dd 0x00000000                          ; table base address

idtr:
  dw (64*8)-1
  dd idt

idt:
  times 64*8 db 0


; IRQ handler wrappers
extern timer_handler
IRQ0_wrapper:
  pushad
  cld
  call timer_handler
  popad
  iret

extern spurious_handler
IRQ31_wrapper:
  pushad
  cld
  call spurious_handler
  popad
  iret

; Install interrupt handler
global install_ISR
install_ISR: use32
  pushad

  ; Configure PIC
  mov  al, 0x11                          ; 0x11 = ICW1_INIT | ICW1_ICW4
  out  0x20, al                          ; send ICW1 to master pic
  out  0xA0, al                          ; send ICW1 to slave pic
  mov  al, 0x08                          ; get master pic vector param
  out  0x21, al                          ; send ICW2 aka vector to master pic
  mov  al, 0x70                          ; get slave pic vector param
  out  0xA1, al                          ; send ICW2 aka vector to slave pic
  mov  al, 0x04                          ; 0x04 = set slave to IRQ2
  out  0x21, al                          ; send ICW3 to master pic
  mov  al, 0x02                          ; 0x02 = tell slave its on IRQ2 of master
  out  0xA1, al                          ; send ICW3 to slave pic
  mov  al, 0x01                          ; 0x01 = ICW4_8086
  out  0x21, al                          ; send ICW4 to master pic
  out  0xA1, al                          ; send ICW4 to slave pic

  mov  al, 0xFF                          ; mask PIC interrupts
  out  0x21, al
  out  0xA1, al

  ; Setup IDT

  ; Syscall
  mov eax, int_handler
  mov [idt+49*8], ax
  mov word [idt+49*8+2], CODE32
  mov word [idt+49*8+4], 0x8F00
  shr eax, 16
  mov [idt+49*8+6], ax

  ; IRQ0 (timer)
  mov eax, IRQ0_wrapper
  mov [idt+32*8], ax
  mov word [idt+32*8+2], CODE32
  mov word [idt+32*8+4], 0x8E00
  shr eax, 16
  mov [idt+32*8+6], ax

  ; IRQ31 (spurious)
  mov eax, IRQ31_wrapper
  mov [idt+63*8], ax
  mov word [idt+63*8+2], CODE32
  mov word [idt+63*8+4], 0x8E00
  shr eax, 16
  mov [idt+63*8+6], ax

  ; Load IDT
  lidt [idtr]

  popad
  ret

; Interrupt int_handler
; Call c function
int_handler: use32
  pushad
  push ecx
  push ebx
  call kernel_service
  mov  [.result], eax
  pop eax
  pop eax
  popad
  mov eax, [.result]
  iretd

.result dd 0                            ; Result of ISR

ALIGN 2
global disk_buff
disk_buff:
  times 512 db 0

extern kernel_service
