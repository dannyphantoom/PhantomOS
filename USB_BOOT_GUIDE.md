# PhantomOS USB Boot Guide

🚀 **Boot PhantomOS on Real Hardware**

This guide will help you create a bootable USB drive and test PhantomOS on actual computer hardware instead of QEMU emulation.

## 🎯 Why Boot from USB?

- **Real Hardware Testing** - Eliminates QEMU-specific issues
- **Better Performance** - Native hardware execution
- **Authentic Experience** - See how your OS behaves on real systems
- **Hardware Compatibility** - Test with different keyboards, displays, etc.

## 📋 Prerequisites

- **USB Drive** - At least 8MB (any size works, 1GB+ recommended)
- **Linux System** - For creating the bootable USB
- **Target Computer** - To boot PhantomOS on
- **BIOS/UEFI Access** - To change boot order

⚠️ **WARNING**: This process will **COMPLETELY ERASE** your USB drive!

## 🔨 Step 1: Create USB Image

```bash
# Build PhantomOS USB image
make usb-image
```

This creates `build/phantom_usb.img` (8MB USB-bootable image).

## 💾 Step 2: Write to USB Drive

### Option A: Use the Safe Script (Recommended)
```bash
# Use the included USB creation script
sudo ./create_usb.sh
```

The script will:
- Show available drives
- Ask you to select the USB drive
- Provide safety confirmations
- Write the image safely

### Option B: Manual Method
```bash
# 1. Find your USB drive
lsblk

# 2. Write image (REPLACE sdX with your USB drive!)
sudo dd if=build/phantom_usb.img of=/dev/sdX bs=4M status=progress

# 3. Sync to ensure write completion
sync
```

**CRITICAL**: Replace `sdX` with your actual USB device (e.g., `sdb`, `sdc`). 
**WRONG DEVICE = DATA LOSS!**

## 🖥️ Step 3: Boot from USB

### Configure BIOS/UEFI
1. **Insert USB** into target computer
2. **Power on** and immediately press BIOS key:
   - **Common keys**: F2, F12, Delete, F1, F10, Esc
   - **Brand specific**:
     - Dell: F2 or F12
     - HP: F2 or F10
     - Lenovo: F1 or F2
     - ASUS: Delete or F2
     - Acer: F2
3. **Navigate to Boot menu**
4. **Select USB drive** from boot order
5. **Save and exit**

### Alternative: Boot Menu
- Many systems allow **one-time boot menu**:
  - Press F12, F11, or F8 during startup
  - Select USB drive directly

## 🎮 Step 4: Test PhantomOS

Once booted, you should see:

```
PhantomOS Simple Bootloader Starting...
Loading kernel from disk...
Kernel loaded successfully
Enabling A20 line...
Entering protected mode, jumping to kernel...

=== PhantomOS 32-bit Kernel ===
32-bit kernel with POSIX file system loaded!

Starting PhantomOS Shell...
Type 'help' for available commands.

phantom:/$ 
```

### Test Commands
```bash
phantom:/$ help           # Show available commands
phantom:/$ ls             # List files
phantom:/$ mkdir test     # Create directory
phantom:/$ cd test        # Change directory
phantom:/$ pwd            # Show current path
phantom:/$ touch file.txt # Create file
phantom:/$ ls             # See the file
phantom:/$ cd ..          # Go back
phantom:/$ version        # Show OS version
```

## 🔧 Troubleshooting

### USB Won't Boot
- ✅ **Try different USB ports** (USB 2.0 often more compatible)
- ✅ **Check BIOS settings**:
  - Enable "USB Boot Support"
  - Enable "Legacy Boot" or "CSM" mode
  - Disable "Secure Boot" if present
- ✅ **Try different USB drives** (some brands work better)
- ✅ **Check boot order** in BIOS

### Keyboard Doesn't Work
- ✅ **Try different keyboards** (PS/2 often more reliable than USB)
- ✅ **Enable USB keyboard support** in BIOS
- ✅ **Try both USB 2.0 and 3.0 ports**

### System Crashes/Hangs
- ✅ **Check RAM compatibility** (try with minimal RAM)
- ✅ **Disable CPU features** in BIOS (like virtualization)
- ✅ **Try on different computers**

### Boot Loops/Resets
- ✅ **Different computer models** (some hardware isn't compatible)
- ✅ **Older hardware** (often more compatible with simple OS)
- ✅ **Check power supply** (ensure stable power)

## 🖥️ Recommended Test Hardware

### ✅ Known Compatible
- **Older Intel systems** (Core 2 Duo era)
- **Basic motherboards** without fancy features
- **PS/2 keyboards** (more reliable than USB)
- **VGA displays** (basic compatibility)

### ⚠️ May Have Issues  
- **Very new hardware** (complex initialization)
- **Gaming motherboards** (too many features)
- **UEFI-only systems** (prefer legacy BIOS)
- **Laptops with custom firmware**

## 🔍 Hardware Testing Results

Document your results:

| Computer Model | CPU | RAM | Boot Success | Keyboard | Notes |
|---------------|-----|-----|--------------|----------|-------|
| Dell OptiPlex | i5  | 4GB | ✅ Yes       | ✅ Works | Perfect |
| Gaming PC     | i7  | 16GB| ❌ No        | N/A      | UEFI issues |

## 🎯 Expected Results vs QEMU

### Real Hardware Advantages:
- **Better keyboard responsiveness**
- **No emulation overhead**
- **Authentic hardware behavior**
- **Real interrupt timing**

### Potential Differences:
- **Different boot sequence**
- **Hardware-specific quirks** 
- **Varying keyboard scan codes**
- **Different memory layouts**

## 🚀 Success Metrics

Your PhantomOS is working correctly if:
- ✅ **Boots to shell prompt**
- ✅ **Keyboard input works**
- ✅ **Commands respond correctly**
- ✅ **File system operations work**
- ✅ **No crashes during normal use**

## 📚 Next Steps

After successful USB boot:
- **Test all POSIX commands**
- **Stress test the file system**
- **Try on multiple computers**
- **Document compatibility**
- **Consider hardware-specific optimizations**

---

**🎉 Congratulations! You've successfully created a bootable operating system that runs on real hardware!**

*This is a significant achievement in systems programming - from broken QEMU emulation to working bare metal OS!* 