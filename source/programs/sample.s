; Sample nas program
; Clear the screen and show "Hello"

ORG 0x20000         ; Origin address

main:
  push ebx          ; save reg state
  push ecx

  ; Clear screen syscall
  mov  ebx, [cls]   ; ebx param to syscall: service code
  mov  ecx, 0x00    ; ecx param to syscall: unused
  int  0x31         ; make syscall

  ; Print string
  mov  ecx, hstr    ; Put initial string address in ecx
  mov  ebx, [ochar] ; out char service code in ebx

repeat:             ; loop for each char
  int  0x31         ; make syscall

  add  ecx, 4       ; inc ecx so it points next char of str
  mov  edx, 0
  cmp  [ecx], edx   ; compare current char and 0
  jne  repeat       ; if not equal, continue loop

  ; Otherwise, we are done
  pop  ecx          ; Pop prevously pushed params
  pop  ebx

  ret               ; Finish

; "Hello\n" string (ascii)
hstr  dd 0x48, 0x65, 0x6C, 0x6C, 0x6F, 10, 0

; syscall codes
cls   dd 0x22    ; clear screen
ochar dd 0x20    ; out char
