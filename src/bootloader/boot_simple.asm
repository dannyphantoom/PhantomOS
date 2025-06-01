[org 0x7c00]
bits 16

start:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax 
    mov sp, 0x7c00

    ; Print startup message
    mov si, msg_start
    call print_both

    ; Load kernel from disk
    call load_kernel

    ; Enable A20 line
    call enable_a20

    ; Enter protected mode (32-bit)
    call enter_protected_mode

; Load kernel from disk with retry
load_kernel:
    mov si, msg_loading
    call print_both
    
    mov byte [disk_tries], 3  ; Try 3 times

.retry:
    ; Reset disk system
    mov ah, 0x00
    mov dl, 0x80    ; Use hard drive instead of floppy
    int 0x13
    
    ; Load kernel (64 sectors = 32KB to accommodate larger kernel with editor)
    mov ah, 0x02    ; Read sectors
    mov al, 64      ; Number of sectors (increased to 64)
    mov ch, 0x00    ; Cylinder 0
    mov cl, 0x02    ; Start from sector 2
    mov dh, 0x00    ; Head 0
    mov dl, 0x80    ; Hard drive 0
    
    ; Set up buffer at 0x10000
    mov bx, 0x1000
    mov es, bx
    mov bx, 0x0000
    
    int 0x13
    jnc .success    ; Jump if no carry (success)
    
    ; Retry if failed
    dec byte [disk_tries]
    jnz .retry
    
    ; All retries failed
    mov si, msg_disk_error
    call print_both
    cli
    hlt

.success:
    mov si, msg_loaded
    call print_both
    ret

; Enable A20 line
enable_a20:
    mov si, msg_a20
    call print_both
    
    ; Try fast A20 gate
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

; Enter 32-bit protected mode
enter_protected_mode:
    mov si, msg_pmode
    call print_both
    
    cli
    lgdt [gdt_descriptor]
    
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    jmp CODE_SEG:protected_mode_start

; Print to both VGA and serial
print_both:
    call print_vga
    call print_serial
    ret

; Print to VGA
print_vga:
    push si
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0e
    int 0x10
    jmp .loop
.done:
    pop si
    ret

; Print to serial
print_serial:
.loop:
    lodsb
    or al, al
    jz .done
    call serial_write
    jmp .loop
.done:
    ret

; Write character to serial
serial_write:
    push dx
    push ax
    mov dx, 0x3FD
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, 0x3F8
    out dx, al
    pop dx
    ret

bits 32
protected_mode_start:
    ; Set up 32-bit segments
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000  ; Set up stack
    
    ; Copy kernel to 1MB (copying 64 sectors = 32KB)
    mov esi, 0x10000  ; Source
    mov edi, 0x100000 ; Destination (1MB)
    mov ecx, 8192     ; 32KB / 4 = 8192 dwords
    rep movsd
    
    ; Jump to kernel entry point (accounting for ELF header offset)
    jmp 0x100000

; GDT for 32-bit protected mode
gdt_start:
    ; Null descriptor
    dq 0x0
    
    ; Code segment (32-bit)
    dw 0xFFFF       ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base mid
    db 0x9A         ; Access: Present, Ring 0, Code, Execute/Read
    db 0xCF         ; Flags: 4KB granularity, 32-bit
    db 0x00         ; Base high
    
    ; Data segment (32-bit)
    dw 0xFFFF       ; Limit low
    dw 0x0000       ; Base low
    db 0x00         ; Base mid
    db 0x92         ; Access: Present, Ring 0, Data, Read/Write
    db 0xCF         ; Flags: 4KB granularity, 32-bit
    db 0x00         ; Base high
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 0x08
DATA_SEG equ 0x10

; Data
disk_tries db 0

; Messages
msg_start db "PhantomOS Simple Bootloader Starting...", 13, 10, 0
msg_loading db "Loading kernel from disk...", 13, 10, 0
msg_loaded db "Kernel loaded successfully", 13, 10, 0
msg_disk_error db "Disk read error after retries!", 13, 10, 0
msg_a20 db "Enabling A20 line...", 13, 10, 0
msg_pmode db "Entering protected mode, jumping to kernel...", 13, 10, 0

; Boot signature
times 510 - ($ - $$) db 0
dw 0xaa55 
