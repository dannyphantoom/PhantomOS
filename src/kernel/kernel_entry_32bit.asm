bits 32

global _start
extern kernel_main
extern __bss_start
extern __bss_end

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
    
    ; Zero the BSS segment
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    xor eax, eax
    cld
    rep stosb

    ; Clear direction flag (already cleared but ensure)
    cld
    
    ; Call our C kernel main function
    call kernel_main
    
    ; If kernel_main returns, halt the system
    cli
    hlt
    
.hang:
    hlt
    jmp .hang 