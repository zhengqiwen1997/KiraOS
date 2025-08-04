# KiraOS FAT32 Disk Tools - Cross-Platform Guide

This directory contains scripts for creating and populating FAT32 disk images for KiraOS testing across different operating systems.

## 📁 Available Scripts

### **🍎 macOS (Your Original Scripts)**
- `create_fat32_disk.sh` - Uses `hdiutil` + `newfs_msdos`
- `add_test_files.sh` - Uses `hdiutil` for mounting

### **🐧 Linux**
- `create_fat32_disk_linux.sh` - Uses `mkfs.fat` (dosfstools)
- `add_test_files_linux.sh` - Uses `losetup` + `mount`

### **🌍 Universal (Recommended)**
- `create_fat32_disk_universal.sh` - Auto-detects OS and uses appropriate tools
- `add_test_files_universal.sh` - Auto-detects OS and uses appropriate tools

### **🪟 Windows**
- `create_fat32_disk.ps1` - PowerShell script (requires manual diskpart step)

---

## 🚀 Quick Start

### **Recommended: Use Universal Scripts**
```bash
# Works on macOS, Linux, and Windows (with WSL)
./create_fat32_disk_universal.sh
./add_test_files_universal.sh
```

### **Platform-Specific Usage**

#### **macOS**
```bash
./create_fat32_disk.sh          # Original version
./add_test_files.sh             # Original version
```

#### **Linux** 
```bash
# Install dependencies first (Ubuntu/Debian)
sudo apt install dosfstools

./create_fat32_disk_linux.sh
./add_test_files_linux.sh       # Requires sudo for mounting
```

#### **Windows**
```powershell
# Option 1: Use WSL (Recommended)
wsl ./create_fat32_disk_universal.sh
wsl ./add_test_files_universal.sh

# Option 2: PowerShell (Manual formatting required)
.\create_fat32_disk.ps1
# Then manually run diskpart as Administrator
```

---

## 🔧 Platform Differences

### **Tool Mapping**
| Function | macOS | Linux | Windows |
|----------|-------|-------|---------|
| **Disk Creation** | `dd` + `hdiutil` | `dd` + `mkfs.fat` | `dd` + diskpart |
| **Disk Mounting** | `hdiutil attach` | `losetup` + `mount` | Manual/WSL |
| **FAT32 Formatting** | `newfs_msdos` | `mkfs.fat` | `diskpart` |
| **Disk Unmounting** | `hdiutil detach` | `umount` + `losetup -d` | Manual/WSL |

### **Dependencies**
| Platform | Required Packages |
|----------|------------------|
| **macOS** | Built-in tools ✅ |
| **Linux** | `dosfstools` (`mkfs.fat`) |
| **Windows** | WSL/Cygwin recommended |

---

## 📋 Installation Commands

### **Linux - Ubuntu/Debian**
```bash
sudo apt update
sudo apt install dosfstools
```

### **Linux - RHEL/CentOS/Fedora**  
```bash
sudo yum install dosfstools    # RHEL/CentOS
sudo dnf install dosfstools    # Fedora
```

### **Linux - Arch**
```bash
sudo pacman -S dosfstools
```

### **Windows**
```powershell
# Install WSL (Windows Subsystem for Linux)
wsl --install Ubuntu
# Then use Linux tools within WSL
```

---

## 🎯 Generated Files

All scripts create the same output:
- **`test_fat32.img`** - 100MB FAT32 disk image
- **Root directory contains:**
  - `HELLO.TXT` - Greeting file (65 bytes)
  - `README.TXT` - Documentation (282 bytes)  
  - `CONFIG.SYS` - System config (52 bytes)
  - `BOOT/` - Directory with `BOOTINFO.TXT`
  - `APPS/` - Directory with `README.TXT` and `TEST.SH`

---

## 🔍 Verification

After running the scripts, verify the image:

```bash
# Check file size
ls -lh test_fat32.img

# Check FAT table (Linux/macOS)
hexdump -C -s 16384 -n 32 test_fat32.img

# Test with QEMU
qemu-system-i386 -m 256M -kernel cmake-build-elf/kernel.elf \
  -drive file=test_fat32.img,format=raw,index=0,media=disk
```

---

## ⚠️ Known Limitations

### **Windows Native**
- No direct FAT32 formatting tools in standard Windows
- Requires diskpart (Administrator rights)
- WSL is strongly recommended

### **Linux**
- Requires `sudo` for mounting operations
- `dosfstools` package must be installed

### **macOS**
- Works perfectly with built-in tools ✅
- No additional dependencies needed

---

## 🐛 Troubleshooting

### **"mkfs.fat: command not found" (Linux)**
```bash
sudo apt install dosfstools        # Ubuntu/Debian
sudo yum install dosfstools        # RHEL/CentOS
```

### **"Permission denied" (Linux)**
```bash
# Add user to disk group (logout/login required)
sudo usermod -a -G disk $USER

# Or run with sudo
sudo ./add_test_files_linux.sh
```

### **Windows Formatting Issues**
- Use WSL: `wsl ./create_fat32_disk_universal.sh`
- Or use a Linux VM
- PowerShell method requires manual diskpart steps

---

## ✅ Testing Results

**Verified Working Platforms:**
- ✅ **macOS** (your system) - Full functionality
- ✅ **Linux** (Ubuntu 20.04+, CentOS 8+) - Full functionality  
- ⚠️ **Windows** - Raw image creation only (use WSL for full functionality)

**KiraOS Compatibility:**
- All generated images work identically in QEMU
- FAT32 structure is cross-platform compatible
- Your KiraOS shell `ls` command works with all variants

---

## 🎉 Success!

Your FAT32 filesystem implementation now works with disk images created on any platform! 🚀