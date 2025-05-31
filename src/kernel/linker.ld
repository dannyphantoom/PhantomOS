ENTRY(_start)

SECTIONS
{
    /* Kernel will be loaded at 1MB mark */
    . = 0x100000;
    
    /* Read-only sections */
    .text ALIGN(4K) : {
        *(.text)
        *(.text.*)
    }
    
    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* Read-write sections */
    .data ALIGN(4K) : {
        *(.data)
        *(.data.*)
    }
    
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    
    /* Mark the end of the kernel */
    kernel_end = .;
} 