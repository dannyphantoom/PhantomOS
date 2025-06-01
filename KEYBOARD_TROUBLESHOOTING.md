# PhantomOS Keyboard Troubleshooting Guide

## Overview
This guide helps resolve keyboard input issues in PhantomOS when running in QEMU or on real hardware.

## Fixed Issues (v0.3)
1. **Compilation Error**: Fixed `-oformat` to `--oformat` in Makefile
2. **PIC Issue**: Fixed position-independent code generation with `-fno-pie -fno-pic`
3. **Initialization Order**: IDT is now initialized before keyboard
4. **PS/2 Controller**: Improved keyboard initialization sequence

## Testing Keyboard Input

### Quick Test
```bash
# Build the OS
make clean && make

# Run the test script
./test_keyboard.sh
```

### Manual Test
```bash
# Run with GTK display
make run

# Or run with console output
make run-console
```

## What to Expect
1. PhantomOS boot message appears
2. Shell prompt: `phantom:/$ `
3. Keyboard input should work immediately
4. Try these commands:
   - `help` - Show available commands
   - `version` - Show OS version
   - `clear` - Clear screen
   - `echo test` - Echo text

## Common Issues and Solutions

### Issue: No keyboard input accepted

**Symptoms**: 
- Can see the OS boot and shell prompt
- Typing does nothing
- No characters appear on screen

**Solutions**:

1. **Check QEMU keyboard grab**:
   - Click inside the QEMU window to grab keyboard focus
   - Press Ctrl+Alt+G to grab/ungrab keyboard

2. **Verify interrupts are enabled**:
   - Look for debug markers on boot: `Z`, `I`, `L`, `S`
   - These indicate: Kernel start, IDT init, IDT loaded, Interrupts enabled

3. **Check IRQ mask**:
   - The kernel shows IRQ1 mask status at position 2 on screen
   - Should show '0' (unmasked) not '1' (masked)

### Issue: Garbage characters or wrong keys

**Symptoms**:
- Keys produce wrong characters
- Special keys don't work

**Solutions**:

1. **Scancode table limited**:
   - Currently only supports basic US QWERTY layout
   - Special keys, function keys not implemented

2. **Key mapping issues**:
   - Check the `scancode_to_ascii` table in kernel.c
   - May need adjustment for your keyboard layout

### Issue: Keyboard works in QEMU but not on real hardware

**Solutions**:

1. **BIOS/UEFI settings**:
   - Enable Legacy USB support
   - Enable PS/2 emulation
   - Disable Fast Boot

2. **USB vs PS/2**:
   - PhantomOS expects PS/2 keyboard
   - USB keyboards need BIOS PS/2 emulation

## Debug Mode

To enable keyboard debug output, modify `kernel.c`:

```c
void keyboard_handler(void) {
    // Uncomment for debug
    // terminal_putchar('K');  // Shows 'K' for each interrupt
    
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // Uncomment to show scancodes
    // terminal_putchar('[');
    // terminal_putchar('0' + (scancode / 10));
    // terminal_putchar('0' + (scancode % 10));
    // terminal_putchar(']');
    
    // ... rest of handler
}
```

## Technical Details

### Keyboard Initialization Sequence
1. Wait for controller ready
2. Disable PS/2 ports
3. Flush output buffer
4. Configure controller (enable IRQ1)
5. Enable first PS/2 port
6. Reset keyboard (0xFF command)
7. Enable scanning (0xF4 command)

### Interrupt Handling
- Keyboard uses IRQ1 (interrupt 0x21 after PIC remapping)
- PIC is remapped: IRQ0-7 → INT 0x20-0x27
- Only keyboard IRQ is unmasked (0xFD mask)

### Current Limitations
1. Basic US QWERTY layout only
2. No shift/caps lock support
3. No numpad or function keys
4. No key repeat
5. Single scancode set (Set 1)

## Building from Source

If you modified the kernel, rebuild with:
```bash
make clean
make
make run  # Test immediately
```

## Need More Help?

1. Check kernel debug output at top-left of screen
2. Try different QEMU versions
3. Test with `-accel tcg` to disable KVM
4. Enable serial debugging: `make run-console` 