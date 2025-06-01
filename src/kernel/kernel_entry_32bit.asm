bits 32

global _start
extern kernel_main

section .text
_start:
    ; Call the C kernel main function
    call kernel_main
    
    ; If kernel_main returns (it shouldn't), halt
    cli
.halt:
    hlt
    jmp .halt 