bits 32

global keyboard_interrupt_handler
extern keyboard_handler

section .text

keyboard_interrupt_handler:
    ; Save all 32-bit registers
    pushad          ; Pushes EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    
    ; Call C keyboard handler
    call keyboard_handler
    
    ; Restore all 32-bit registers
    popad           ; Pops EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX
    
    ; Return from interrupt (32-bit)
    iret 