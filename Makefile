# Makefile for Hospital POS System

AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy

CFLAGS = -ffreestanding -O2 -Wall -Wextra -std=c99 -nostdlib -Iinclude
ASFLAGS = -f elf32
LDFLAGS = -T linker.ld -nostdlib

TARGET = hosp_pos.bin
ISO = hosp_pos.iso

OBJS = \
    boot/bootsect.o \
    kernel/kernel.o \
    kernel/interrupts.o \
    kernel/memory.o \
    kernel/task.o \
    modules/doctor.o \
    modules/medication.o \
    modules/cashier.o \
    modules/reception.o \
    modules/warehouse.o \
    utils/vga.o \
    utils/keyboard.o \
    utils/string.o \
    utils/math.o \
    utils/file.o \
    ipc/ipc.o \
    drivers/disk.o \
    drivers/rtc.o \
    drivers/parallel.o

all: $(TARGET) $(ISO)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o hosp_pos.elf $(OBJS)
	$(OBJCOPY) -O binary hosp_pos.elf $(TARGET)

%.o: %.asm
	$(AS) $(ASFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(ISO): $(TARGET)
	mkdir -p iso/boot
	cp $(TARGET) iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o $(ISO) iso

clean:
	rm -f $(OBJS) $(TARGET) hosp_pos.elf $(ISO)
	rm -rf iso

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 4M

debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 4M -s -S &
	gdb -ex "target remote localhost:1234" -ex "symbol-file hosp_pos.elf"

.PHONY: all clean run debug
