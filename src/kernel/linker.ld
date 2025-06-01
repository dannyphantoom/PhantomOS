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

    /* GDT section (must be loaded in memory) */
    .gdt ALIGN(8) : {
        *(.gdt)
    }
    
    .bss ALIGN(4K) : {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    }
    
    /* Mark the end of the kernel */
    kernel_end = .;
} 