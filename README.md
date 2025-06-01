# PhantomOS

![PhantomOS Logo](https://img.shields.io/badge/PhantomOS-v0.3.1-blue?style=for-the-badge)
![Architecture](https://img.shields.io/badge/Architecture-32--bit-green?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

A **32-bit operating system** with POSIX-compatible file system and interactive shell, built from scratch in Assembly and C.

## 🎯 Project Overview

PhantomOS is a custom operating system that demonstrates:
- **Bootloader development** - Multi-stage boot process from 16-bit real mode to 32-bit protected mode
- **Kernel development** - Freestanding C kernel with VGA text mode output
- **File system implementation** - In-memory POSIX-compatible file system
- **Interactive shell** - Real-time keyboard input with Unix-standard commands
- **Interrupt handling** - PIC configuration and keyboard interrupt processing

## ✨ Features

### 🔧 System Components
- **32-bit Protected Mode Kernel** - Stable, reliable architecture
- **VGA Text Mode Output** - 80x25 color terminal display
- **Keyboard Input Handling** - Real-time scancode to ASCII translation
- **Interrupt System** - IDT setup with PIC configuration
- **Memory Management** - Simple allocator with 64KB memory pool

### 📁 POSIX File System
- **Hierarchical Directory Structure** - Unix-style navigation with `/`, `.`, `..`
- **In-Memory Storage** - 64KB total capacity, 128 max files
- **File Operations** - Create, read, write, copy, move, delete
- **Directory Management** - Create and remove directories
- **Path Resolution** - Full absolute and relative path support

### 🖥️ Shell Commands
All standard POSIX commands are supported:

| Command | Description |
|---------|-------------|
| `ls [dir]` | List directory contents |
| `pwd` | Print working directory |
| `cd <dir>` | Change directory |
| `mkdir <dir>` | Create directory |
| `rmdir <dir>` | Remove empty directory |
| `touch <file>` | Create empty file |
| `rm <file>` | Remove file |
| `cp <src> <dest>` | Copy file |
| `mv <src> <dest>` | Move/rename file |
| `cat <file>` | Display file contents |
| `echo <text>` | Echo text to terminal |
| `clear` | Clear screen |
| `help` | Show available commands |
| `version` | Display OS version |
| `exit` | Halt system |

## 🚀 Quick Start

### Prerequisites
- Linux development environment
- `nasm` (Netwide Assembler)
- `gcc` (GNU Compiler Collection) 
- `make` (Build system)
- `qemu-system-x86_64` (Emulator)

### Build and Run
```bash
# Clone or download the project
cd PhantomOS

# Build the OS
make all

# Run in QEMU
make run

# Test keyboard functionality
./test_keyboard.sh

# Clean build files
make clean
```

### First Boot
When PhantomOS boots, you'll see:
```
=== PhantomOS 32-bit Kernel ===
32-bit kernel with POSIX file system loaded!

Kernel initialized with:
  - VGA text mode output
  - Keyboard input handling  
  - Interrupt system
  - In-memory file system
  - POSIX-compatible shell commands

Starting PhantomOS Shell...
Type 'help' for available commands.

phantom:/$ 
```

## 🛠️ Recent Fixes (v0.3.1)

### Compilation Errors Fixed
- **Linker Flag**: Corrected `-oformat` to `--oformat` in Makefile
- **PIC Issues**: Added `-fno-pie -fno-pic` flags to prevent position-independent code generation

### Keyboard Input Fixed
- **PS/2 Controller**: Improved initialization sequence with proper status checks
- **Initialization Order**: IDT is now set up before keyboard initialization
- **Interrupt Handling**: Enhanced keyboard interrupt handler
- **Debug Output**: Removed debug characters that interfered with input

### Documentation Added
- **Troubleshooting Guide**: See `KEYBOARD_TROUBLESHOOTING.md` for keyboard issues
- **Test Script**: `test_keyboard.sh` for easy keyboard testing

## 🔧 Technical Architecture

### Boot Process
1. **BIOS** loads 512-byte bootloader from sector 1
2. **Bootloader** (`boot_simple.asm`)
   - Sets up segments and stack
   - Loads kernel from disk (32 sectors = 16KB)
   - Enables A20 line for extended memory access
   - Enters 32-bit protected mode
   - Copies kernel to 1MB memory mark
   - Jumps to kernel entry point
3. **Kernel** (`kernel_entry_32bit.asm` → `kernel.c`)
   - Initializes VGA text mode
   - Sets up interrupt system (IDT + PIC)
   - Initializes file system
   - Starts interactive shell

### Memory Layout
```
0x00000000 - 0x000003FF : Interrupt Vector Table
0x00000400 - 0x000007FF : BIOS Data Area  
0x00007C00 - 0x00007DFF : Bootloader (512 bytes)
0x00010000 - 0x0001FFFF : Kernel loading area
0x00090000 - 0x0009FFFF : Stack space
0x000A0000 - 0x000BFFFF : Video memory
0x00100000 - 0x0010FFFF : Kernel runtime location (1MB mark)
```

### File System Specifications
- **Total Capacity**: 64KB memory pool
- **Max Files**: 128 total files/directories  
- **Max Directory Size**: 32 files per directory
- **Max File Size**: 4KB per file
- **Path Length**: 256 characters maximum
- **Filename Length**: 64 characters maximum

## 📂 Project Structure

```
PhantomOS/
├── README.md                    # This file
├── KEYBOARD_TROUBLESHOOTING.md  # Keyboard debugging guide
├── Makefile                     # Build system
├── test_keyboard.sh             # Keyboard test script
├── src/
│   ├── bootloader/
│   │   └── boot_simple.asm      # 32-bit bootloader
│   └── kernel/
│       ├── kernel.c             # Main kernel with shell
│       ├── kernel.h             # Kernel headers
│       ├── kernel_entry_32bit.asm # Kernel entry point
│       ├── filesystem.c         # POSIX file system
│       ├── filesystem.h         # File system headers
│       ├── interrupts.asm       # Keyboard interrupt handlers
│       └── linker.ld           # Memory layout script
└── build/                       # Generated files (created by make)
    ├── boot.bin                 # Compiled bootloader
    ├── kernel.bin               # Compiled kernel
    └── os.img                   # Final OS image
```

## 🎮 Usage Examples

### Basic Navigation
```bash
phantom:/$ ls          # List root directory
phantom:/$ mkdir test  # Create directory
phantom:/$ cd test     # Enter directory  
phantom:/test$ pwd     # Show current path
/test
phantom:/test$ cd ..   # Go back to parent
phantom:/$ 
```

### File Operations
```bash
phantom:/$ touch hello.txt        # Create file
phantom:/$ echo "Hello World"     # Echo text
Hello World
phantom:/$ ls                     # List files
hello.txt
phantom:/$ rm hello.txt           # Remove file
phantom:/$ ls                     # Verify removal

phantom:/$ 
```

### Directory Management
```bash
phantom:/$ mkdir projects         # Create directory
phantom:/$ mkdir projects/test    # Create subdirectory  
phantom:/$ cd projects/test       # Navigate to subdirectory
phantom:/projects/test$ pwd       # Show full path
/projects/test
```

## 🚧 Known Limitations

- **32-bit Architecture**: Limited to 4GB address space (sufficient for educational purposes)
- **In-Memory Only**: No persistent storage (files lost on reboot)
- **Single Tasking**: No multitasking or process management
- **Basic Memory Management**: Simple allocator without deallocation
- **Limited Hardware Support**: VGA text mode only, no graphics
- **Basic Keyboard**: US QWERTY layout only, no shift/caps lock

## 🔮 Future Enhancements

- [ ] **64-bit Architecture**: Stable long mode implementation
- [ ] **Persistent Storage**: IDE/SATA disk driver with filesystem persistence  
- [ ] **Multitasking**: Process scheduling and task switching
- [ ] **Virtual Memory**: Paging and memory protection
- [ ] **Network Stack**: Basic TCP/IP implementation
- [ ] **Graphics Mode**: VESA framebuffer support
- [ ] **Enhanced Keyboard**: Full keyboard support with modifiers

## 🤝 Contributing

Contributions are welcome! Areas for improvement:
- Hardware driver development
- Memory management enhancements  
- File system optimizations
- Shell feature additions
- Documentation improvements

## 📜 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🏆 Acknowledgments

- **OSDev Community** - Invaluable resources and documentation
- **Intel Architecture Manuals** - Complete x86 reference
- **QEMU Project** - Excellent emulation platform for development
- **NASM Documentation** - Assembly language reference

---

**Built with ❤️ by a systems programming enthusiast**

*PhantomOS demonstrates that with determination and proper debugging, even the most complex boot issues can be resolved!*

### Recent Updates

- **Vim-like Text Editor**: Added a built-in text editor with vim-like keybindings
  - Commands: `edit filename` or `vi filename`
  - Modes: Normal, Insert, and Command modes
  - Basic vim commands: i, a, o, h/j/k/l, x, dd, :w, :q, :wq
- **Kernel Size**: The kernel has grown to ~29KB with the editor. The bootloader loads 32KB.
- **Memory Optimization**: Editor buffer reduced to 50 lines x 76 chars to save space

### What Works

// ... existing code ... 