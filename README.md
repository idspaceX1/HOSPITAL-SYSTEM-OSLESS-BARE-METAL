# Hospital Management System (Bare-Metal/OS-less)
A high-performance, lightweight hospital management system designed to run directly on x86 hardware without an underlying Operating System. This project demonstrates low-level systems programming, custom bootloading, and bare-metal resource management.
## 🚀 Overview
Unlike traditional software that relies on Windows or Linux, this system acts as its own kernel. It handles hardware initialization, memory management, and process logic internally, providing a dedicated environment for hospital operations.
Key Modules
 * reception.c: Patient registration and check-in flow.
 * doctor.c: Consultation interface and medical record management.
 * warehouse.c & medication.c: Inventory tracking and pharmaceutical supply chain.
 * cashier.c: Billing and POS (Point of Sale) integration.
 * ipc.c: Custom Inter-Process Communication mechanism for module synchronization.
## 🛠 Technical Architecture
 * Bootloader (bootloader.asm): A custom 16-bit real-mode bootloader that transitions the CPU to 32-bit Protected Mode and loads the kernel.
 * Kernel (kernel.c): The core engine managing the system lifecycle, I/O, and module execution.
 * Hardware Abstraction: Custom implementations for VGA text-mode output, keyboard interrupts, and memory mapping.
 * Linker Script (linker.id): Defines the memory layout to ensure the binary is correctly placed in physical RAM.

## ⚙️ Prerequisites
To build and run this system, you need:
 * NASM: Netwide Assembler for assembly files.
 * GCC: Cross-compiler for x86 (e.g., i686-elf-gcc).
 * QEMU: For hardware emulation and testing.
 * Make: To automate the build process.
## 🔨 Installation & Build
 * Clone the repository:
```
   git clone https://github.com/idspaceX1/HOSPITAL-SYSTEM-OSLESS-BARE-METAL.git
cd HOSPITAL-SYSTEM-OSLESS-BARE-METAL
```
 * Compile the system:
```
   make
```
 * Run in QEMU:
```
   qemu-system-i386 -drive format=raw,file=hospital_system.bin
```

## 📑 Roadmap
 * [ ] Implement a basic FAT12/16 File System for persistent data storage.
 * [ ] Add network support for remote database synchronization.
 * [ ] Enhance UI using custom GUI drivers instead of VGA Text Mode.
## 📄 License
This project is licensed under the MIT License - see the LICENSE file for details.
Disclaimer: This is a bare-metal educational project. It is intended for hardware interaction studies and is not recommended for actual medical data storage without further security and stability implementations.
