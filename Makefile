# PhantomOS Build System

# Compiler and assembler settings
ASM = nasm
CC = gcc
LD = ld

# Directories
BOOTLOADER_DIR = src/bootloader
KERNEL_DIR = src/kernel
BUILD_DIR = build

# Compiler flags for kernel (32-bit, no standard library)
CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -nostdinc

# Linker flags
LDFLAGS = -T $(KERNEL_DIR)/linker.ld -nostdlib -melf_i386

# Target files
BOOTLOADER = $(BUILD_DIR)/boot.bin
KERNEL = $(BUILD_DIR)/kernel.bin
OS_IMAGE = $(BUILD_DIR)/os.img

# Object files
KERNEL_ASM_OBJ = $(BUILD_DIR)/kernel_entry.o
KERNEL_INTERRUPTS_OBJ = $(BUILD_DIR)/interrupts.o
KERNEL_C_OBJ = $(BUILD_DIR)/kernel.o
KERNEL_FS_OBJ = $(BUILD_DIR)/filesystem.o

.PHONY: all clean run

all: $(OS_IMAGE)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build working bootloader (32-bit)
$(BOOTLOADER): $(BOOTLOADER_DIR)/boot_simple.asm | $(BUILD_DIR)
	$(ASM) -f bin -o $@ $<

# Build kernel assembly entry point (32-bit)
$(KERNEL_ASM_OBJ): $(KERNEL_DIR)/kernel_entry_32bit.asm | $(BUILD_DIR)
	$(ASM) -f elf32 -o $@ $<

# Build kernel interrupt handlers (32-bit)
$(KERNEL_INTERRUPTS_OBJ): $(KERNEL_DIR)/interrupts.asm | $(BUILD_DIR)
	$(ASM) -f elf32 -o $@ $<

# Build kernel C code (full version with file system)
$(KERNEL_C_OBJ): $(KERNEL_DIR)/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Build filesystem C code
$(KERNEL_FS_OBJ): $(KERNEL_DIR)/filesystem.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Link kernel (full version with file system, 32-bit)
$(KERNEL): $(KERNEL_ASM_OBJ) $(KERNEL_INTERRUPTS_OBJ) $(KERNEL_C_OBJ) $(KERNEL_FS_OBJ) $(KERNEL_DIR)/linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_ASM_OBJ) $(KERNEL_INTERRUPTS_OBJ) $(KERNEL_C_OBJ) $(KERNEL_FS_OBJ)

# Create OS image (bootloader + kernel)
$(OS_IMAGE): $(BOOTLOADER) $(KERNEL) | $(BUILD_DIR)
	# Create a 1.44MB disk image
	dd if=/dev/zero of=$@ bs=1024 count=1440 2>/dev/null
	# Write bootloader to first sector
	dd if=$(BOOTLOADER) of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	# Write kernel starting at sector 2
	dd if=$(KERNEL) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null

clean:
	rm -rf $(BUILD_DIR)

# Run with QEMU using hard drive interface
run: $(OS_IMAGE)
	qemu-system-x86_64 -drive format=raw,file=$(OS_IMAGE),if=ide,index=0 -display gtk -no-reboot

# Run with QEMU in console mode (with serial, hard drive interface)
run-console: $(OS_IMAGE)
	qemu-system-x86_64 -drive format=raw,file=$(OS_IMAGE),if=ide,index=0 -serial stdio -display none

# Debug with QEMU (enables GDB)
debug: $(OS_IMAGE)
	qemu-system-x86_64 -drive format=raw,file=$(OS_IMAGE),if=ide,index=0 -s -S -no-reboot

# Show build info
info:
	@echo "PhantomOS Build Information:"
	@echo "  Bootloader: $(BOOTLOADER)"
	@echo "  Kernel: $(KERNEL)"
	@echo "  OS Image: $(OS_IMAGE)"
	@echo "  Build directory: $(BUILD_DIR)"
	