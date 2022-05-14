.code32
.section .text.prologue

.global _start
_start:
    movl $stack, %esp
    andl $-16, %esp
    movl $0xDEADBEEF, %eax
    pushl %esp
    pushl %eax
    cli
    call _main

.section .text
.align 4

.section .data
.align 32
stack_begin:
    .fill 0x4000
stack:
