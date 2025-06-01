#!/bin/bash

echo "==========================================="
echo "Testing PhantomOS Keyboard Input"
echo "==========================================="
echo ""
echo "Starting QEMU..."
echo "- Press keys to test keyboard input"
echo "- Try typing commands like 'help', 'version', 'clear'"
echo "- Press Ctrl+C to exit QEMU"
echo ""
echo "==========================================="

# Run QEMU with GTK display to test keyboard input
qemu-system-x86_64 -drive format=raw,file=build/os.img,if=ide,index=0 -display gtk -no-reboot 