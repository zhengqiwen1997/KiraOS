# KiraOS Development Tools

This directory contains various tools and utilities for KiraOS development and testing.

## 📁 Directory Structure

### `/disk/` - FAT32 Disk Image Tools
Cross-platform scripts for creating and populating FAT32 disk images for KiraOS testing.

- **Universal Scripts (Recommended):**
  - `create_fat32_disk_universal.sh` - Auto-detects OS, creates FAT32 image  
  - `add_test_files_universal.sh` - Auto-detects OS, adds test files

- **Platform-Specific Scripts:**
  - `create_fat32_disk.sh` - macOS version (original)
  - `add_test_files.sh` - macOS version (original)
  - `create_fat32_disk_linux.sh` - Linux version
  - `add_test_files_linux.sh` - Linux version
  - `create_fat32_disk.ps1` - Windows PowerShell version

- **Documentation:**
  - `DISK_TOOLS_README.md` - Comprehensive cross-platform guide

## 🚀 Quick Start

From the project root directory:

```bash
# Create FAT32 disk image with test files (universal)
./tools/disk/create_fat32_disk_universal.sh
./tools/disk/add_test_files_universal.sh

# Test with KiraOS
qemu-system-i386 -m 256M -kernel cmake-build-elf/kernel.elf \
  -drive file=test_fat32.img,format=raw,index=0,media=disk
```

## 📋 Requirements by Platform

- **macOS:** Built-in tools ✅
- **Linux:** `dosfstools` package (`sudo apt install dosfstools`)
- **Windows:** WSL recommended, or use PowerShell version

## 🎯 Generated Output

All tools create a `test_fat32.img` file (100MB) containing:
- `HELLO.TXT` - Simple greeting file
- `README.TXT` - Documentation file  
- `CONFIG.SYS` - System configuration
- `BOOT/` - Directory with boot files
- `APPS/` - Directory with application files

Perfect for testing KiraOS's FAT32 filesystem implementation! 🎉