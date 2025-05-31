#!/bin/bash

# PhantomOS USB Creator Script
# Creates a bootable USB drive with PhantomOS

set -e

echo "========================================"
echo "🚀 PhantomOS USB Creator"
echo "========================================"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "❌ This script needs to be run as root (use sudo)"
    echo "   Usage: sudo $0"
    exit 1
fi

# Check if USB image exists
USB_IMAGE="build/phantom_usb.img"
if [ ! -f "$USB_IMAGE" ]; then
    echo "❌ USB image not found: $USB_IMAGE"
    echo "   Run 'make usb-image' first to create the USB image"
    exit 1
fi

echo "📋 Available storage devices:"
echo "========================================"
lsblk -d -o NAME,SIZE,MODEL | grep -E "(sd|nvme)"
echo "========================================"
echo

echo "⚠️  WARNING: This will COMPLETELY ERASE the selected USB drive!"
echo "⚠️  All data on the drive will be permanently lost!"
echo

read -p "🔍 Enter USB device (e.g., sdb, sdc): " USB_DEVICE

# Validate device name
if [[ ! "$USB_DEVICE" =~ ^sd[a-z]$ ]] && [[ ! "$USB_DEVICE" =~ ^nvme[0-9]+n[0-9]+$ ]]; then
    echo "❌ Invalid device name: $USB_DEVICE"
    echo "   Please enter just the device name (e.g., sdb, not /dev/sdb)"
    exit 1
fi

USB_PATH="/dev/$USB_DEVICE"

# Check if device exists
if [ ! -b "$USB_PATH" ]; then
    echo "❌ Device $USB_PATH does not exist"
    exit 1
fi

# Show device info
echo
echo "📊 Device Information:"
echo "========================================"
lsblk "$USB_PATH"
echo "========================================"

# Final confirmation
echo
echo "🔥 FINAL WARNING:"
echo "   Device: $USB_PATH"
echo "   This will PERMANENTLY ERASE all data on $USB_PATH"
echo

read -p "   Type 'YES' to continue (anything else cancels): " CONFIRM

if [ "$CONFIRM" != "YES" ]; then
    echo "❌ Operation cancelled"
    exit 1
fi

echo
echo "🔄 Writing PhantomOS to $USB_PATH..."
echo "   This may take a few minutes..."

# Unmount any mounted partitions
umount ${USB_PATH}* 2>/dev/null || true

# Write the image
dd if="$USB_IMAGE" of="$USB_PATH" bs=4M status=progress oflag=sync

echo
echo "✅ SUCCESS: PhantomOS written to $USB_PATH"
echo
echo "🖥️  NEXT STEPS:"
echo "   1. Safely remove the USB drive"
echo "   2. Insert into target computer"
echo "   3. Boot and select USB in BIOS/UEFI boot menu"
echo "   4. PhantomOS should load with interactive shell"
echo
echo "💡 TROUBLESHOOTING:"
echo "   - If it doesn't boot, try different USB ports"
echo "   - Check BIOS settings for USB boot support"
echo "   - Some computers need 'Legacy Boot' mode"
echo "   - Try different USB drives if one doesn't work"
echo
echo "🎉 Happy hacking with PhantomOS!"
echo "========================================" 