bits 32

global _start
extern kernel_main

section .text
_start:
    ; We're in 32-bit protected mode from the bootloader
    ; Set up stack
    mov esp, 0x90000
    
    ; Set up segments
    mov ax, 0x10    ; Data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Clear direction flag
    cld
    
    ; Call our C kernel main function
    call kernel_main
    
    ; If kernel_main returns, halt the system
    cli
    hlt
    
.hang:
    hlt
    jmp .hang 