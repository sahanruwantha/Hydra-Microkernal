CC=gcc
LD=ld
ASM=nasm

CFLAGS=-m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -I.
LDFLAGS=-T linker.ld -nostdlib -m elf_i386
ASMFLAGS=-f elf32

BUILD_DIR=build
KERNEL_SRCS=$(wildcard kernel/*.c)
COMMON_SRCS=$(wildcard include/*.c)
ASM_SRCS=$(wildcard kernel/*.s)
KERNEL_OBJS=$(KERNEL_SRCS:kernel/%.c=$(BUILD_DIR)/%.o)
COMMON_OBJS=$(COMMON_SRCS:include/%.c=$(BUILD_DIR)/%.o)
ASM_OBJS=$(ASM_SRCS:kernel/%.s=$(BUILD_DIR)/%.o)
ALL_OBJS=$(KERNEL_OBJS) $(COMMON_OBJS) $(ASM_OBJS)
KERNEL_BIN=$(BUILD_DIR)/kernel.bin
ISO_DIR=$(BUILD_DIR)/isodir
BOOT_DIR=$(ISO_DIR)/boot
GRUB_DIR=$(BOOT_DIR)/grub
ISO_FILE=$(BUILD_DIR)/PakayaOs.iso

.PHONY: all build clean create_dirs

all: os-image

build: os-image

create_dirs:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(GRUB_DIR)

$(BUILD_DIR)/%.o: kernel/%.c create_dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: include/%.c create_dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: kernel/%.s create_dirs
	$(ASM) $(ASMFLAGS) $< -o $@

$(KERNEL_BIN): $(ALL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

os-image: $(KERNEL_BIN)
	cp grub/grub.cfg $(GRUB_DIR)
	cp $(KERNEL_BIN) $(BOOT_DIR)/kernel
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

run: os-image
	qemu-system-i386 -boot d -cdrom $(ISO_FILE) -m 512M -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD_DIR)
