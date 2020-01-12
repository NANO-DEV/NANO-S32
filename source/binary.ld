SECTIONS
{
  .text :
  {
    *(.text.startup .text.startup.*)
    *(.text .stub .text.*)
  }
  .data : {*(.data)}
  .bss : {*(.bss)}
  .rodata : {*(.rodata .rodata.* .gnu.linkonce.r.*)}
  /DISCARD/ : {*(.eh_frame .note.GNU-stack)}
}