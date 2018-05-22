; Several x86 utils

[bits 32]

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

%define GDTENTRY(x)                            ((x) << 3)
%define CODE32                                 GDTENTRY(1)	; 0x08
%define DATA32                                 GDTENTRY(2)	; 0x10
%define CODE16                                 GDTENTRY(3)	; 0x18
%define DATA16                                 GDTENTRY(4)	; 0x20
%define STACK16                                k16_stack

section .text

; int32 call makes its POSSIBLE to execute BIOS interrupts
; by temporally switching to virtual 8086 mode
;
; Adapted from original work by Napalm (thank you!)
; License: http://creativecommons.org/licenses/by-sa/2.0/uk/
;
; Notes: int32() resets all selectors
;	void _cdelc int32(uint8_t intnum, regs16_t* regs);
;
global int32
	int32: use32
		cli                                    ; disable interrupts
		pusha                                  ; save register state to 32bit stack
		cld                                    ; clear direction flag (so we copy forward)
		mov  [stack32_ptr], esp                ; save 32bit stack pointer
		sidt [idt32_ptr]                       ; save 32bit idt pointer
		sgdt [gdt32_ptr]                       ; save 32bit gdt pointer
		lgdt [gdt16_ptr]                       ; load 16bit gdt pointer
		lea  esi, [esp+0x24]                   ; set position of intnum on 32bit stack
		lodsd                                  ; read intnum into eax
		mov  [ib], al                          ; set interrupt immediate byte from arguments
		mov  esi, [esi]                        ; read regs pointer in esi as source
		mov  edi, STACK16                      ; set destination to 16bit stack
		mov  ecx, regs16_t_size                ; set copy size to our struct size
		mov  esp, edi                          ; save destination to as 16bit stack offset
		rep  movsb                             ; do the actual copy (32bit stack to 16bit stack)
		jmp  word CODE16:p_mode16              ; switch to 16bit selector (16bit protected mode)
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
		jmp  word 0x0000:r_mode16              ; finally set cs:ip to enter real-mode
	r_mode16: use16
		xor  ax, ax                            ; set ax to zero
		mov  ds, ax                            ; set ds so we can access idt16
		mov  ss, ax                            ; set ss so they the stack is valid
		lidt [idt16_ptr]                       ; load 16bit idt
		mov  bx, 0x0870                        ; master 8 and slave 112
		call resetpic                          ; set pic's the to real-mode settings
		popa                                   ; load general purpose registers from 16bit stack
		pop  gs                                ; load gs from 16bit stack
		pop  fs                                ; load fs from 16bit stack
		pop  es                                ; load es from 16bit stack
		pop  ds                                ; load ds from 16bit stack
		sti                                    ; enable interrupts
		db 0xCD                                ; opcode of INT instruction with immediate byte
	ib: db 0x00
		cli                                    ; disable interrupts
		mov  sp, 0                             ; zero sp so we can reuse it
		mov  ss, sp                            ; set ss so the stack is valid
		mov  sp,k16_stack_top                  ; set correct stack position so we can copy back
		pushf                                  ; save eflags to 16bit stack
		push ds                                ; save ds to 16bit stack
		push es                                ; save es to 16bit stack
		push fs                                ; save fs to 16bit stack
		push gs                                ; save gs to 16bit stack
		pusha                                  ; save general purpose registers to 16bit stack
		mov  bx, 0x2028                        ; master 32 and slave 40
		call resetpic                          ; restore the pic's to protected mode settings
		mov  eax, cr0                          ; get cr0 so we can modify it
		inc  eax                               ; set PE bit to turn on protected mode
		mov  cr0, eax                          ; set cr0 to result
		jmp  dword CODE32:p_mode32             ; switch to 32bit selector (32bit protected mode)
	p_mode32: use32
		mov  ax, DATA32                        ; get our 32bit data selector
		mov  ds, ax                            ; reset ds selector
		mov  es, ax                            ; reset es selector
		mov  fs, ax                            ; reset fs selector
		mov  gs, ax                            ; reset gs selector
		mov  ss, ax                            ; reset ss selector
		lgdt [gdt32_ptr]                       ; restore 32bit gdt pointer
		lidt [idt32_ptr]                       ; restore 32bit idt pointer
		mov  esp, [stack32_ptr]                ; restore 32bit stack pointer
		mov  esi, STACK16                      ; set copy source to 16bit stack
		lea  edi, [esp+0x28]                   ; set position of regs pointer on 32bit stack
		mov  edi, [edi]                        ; use regs pointer in edi as copy destination
		mov  ecx, regs16_t_size                ; set copy size to our struct size
		cld                                    ; clear direction flag (so we copy forward)
		rep  movsb                             ; do the actual copy (16bit stack to 32bit stack)
		popa                                   ; restore registers
	;	sti                                    ; enable interrupts
		ret                                    ; return to caller

	resetpic:                                ; reset's 8259 master and slave pic vectors
		push ax                                ; expects bh = master vector, bl = slave vector
		mov  al, 0x11                          ; 0x11 = ICW1_INIT | ICW1_ICW4
		out  0x20, al                          ; send ICW1 to master pic
		out  0xA0, al                          ; send ICW1 to slave pic
		mov  al, bh                            ; get master pic vector param
		out  0x21, al                          ; send ICW2 aka vector to master pic
		mov  al, bl                            ; get slave pic vector param
		out  0xA1, al                          ; send ICW2 aka vector to slave pic
		mov  al, 0x04                          ; 0x04 = set slave to IRQ2
		out  0x21, al                          ; send ICW3 to master pic
		shr  al, 1                             ; 0x02 = tell slave its on IRQ2 of master
		out  0xA1, al                          ; send ICW3 to slave pic
		shr  al, 1                             ; 0x01 = ICW4_8086
		out  0x21, al                          ; send ICW4 to master pic
		out  0xA1, al                          ; send ICW4 to slave pic
		pop  ax                                ; restore ax from stack
		ret                                    ; return to caller

	stack32_ptr:                             ; address in 32bit stack after we
		dd 0x00000000                          ;   save all general purpose registers

	idt32_ptr:                               ; IDT table pointer for 32bit access
		dw 0x0000                              ; table limit (size)
		dd 0x00000000                          ; table base address

	gdt32_ptr:                               ; GDT table pointer for 32bit access
		dw 0x0000                              ; table limit (size)
		dd 0x00000000                          ; table base address

	idt16_ptr:                               ; IDT table pointer for 16bit access
		dw 0x03FF                              ; table limit (size)
		dd 0x00000000                          ; table base address

	gdt16_base:                              ; GDT descriptor table
		.null:                                 ; 0x00 - null segment descriptor
			dd 0x00000000                        ; must be left zero'd
			dd 0x00000000                        ; must be left zero'd

		.code32:                               ; 0x01 - 32bit code segment descriptor 0xFFFFFFFF
			dw 0xFFFF                            ; limit  0:15
			dw 0x0000                            ; base   0:15
			db 0x00                              ; base  16:23
			db 0x9A                              ; present, iopl/0, code, execute/read
			db 0xCF                              ; 4Kbyte granularity, 32bit selector; limit 16:19
			db 0x00                              ; base  24:31

		.data32:                               ; 0x02 - 32bit data segment descriptor 0xFFFFFFFF
			dw 0xFFFF                            ; limit  0:15
			dw 0x0000                            ; base   0:15
			db 0x00                              ; base  16:23
			db 0x92                              ; present, iopl/0, data, read/write
			db 0xCF                              ; 4Kbyte granularity, 32bit selector; limit 16:19
			db 0x00                              ; base  24:31

		.code16:                               ; 0x03 - 16bit code segment descriptor 0x000FFFFF
			dw 0xFFFF                            ; limit  0:15
			dw 0x0000                            ; base   0:15
			db 0x00                              ; base  16:23
			db 0x9A                              ; present, iopl/0, code, execute/read
			db 0x0F                              ; 1Byte granularity, 16bit selector; limit 16:19
			db 0x00                              ; base  24:31

		.data16:                               ; 0x04 - 16bit data segment descriptor 0x000FFFFF
			dw 0xFFFF                            ; limit  0:15
			dw 0x0000                            ; base   0:15
			db 0x00                              ; base  16:23
			db 0x92                              ; present, iopl/0, data, read/write
			db 0x0F                              ; 1Byte granularity, 16bit selector; limit 16:19
			db 0x00                              ; base  24:31

	gdt16_ptr:                               ; GDT table pointer for 16bit access
		dw gdt16_ptr - gdt16_base - 1          ; table limit (size)
		dd gdt16_base                          ; table base address

idtr:
  dw (50*8)-1
  dd idt

;
; Interrupt int_handler
; Call c function
;
int_handler:
	pushad
	push ecx
	push ebx
	call kernel_service
	mov  [.result], eax
	pop eax
	pop eax
	popad
	mov eax, [.result]
	iret

.result dd 0              ; Result of ISR

extern kernel_service

;
; Install interrupt handler
;
global install_ISR
install_ISR:
  lidt [idtr]
  mov eax, int_handler
  mov [idt+49*8], ax
  mov word [idt+49*8+2], CODE32
  mov word [idt+49*8+4], 0x8E00
  shr eax, 16
  mov [idt+49*8+6], ax
	ret


SECTION .bss

k16_stack:
	resb regs16_t_size       ; kernel16 stack
k16_stack_top:

idt:
	resd 50*2
