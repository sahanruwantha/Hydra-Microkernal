CC=gcc
LD=ld

CFLAGS=-m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic
LDFLAGS=-T linker.ld -nostdlib -m elf_i386

BUILD_DIR=build
KERNEL_OBJ=$(BUILD_DIR)/kernel.o
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

$(KERNEL_OBJ): kernel/kernel.c create_dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

os-image: $(KERNEL_BIN)
	cp grub/grub.cfg $(GRUB_DIR)
	cp $(KERNEL_BIN) $(BOOT_DIR)/kernel
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

run: os-image
	qemu-system-i386 -boot d -cdrom $(ISO_FILE) -m 512M -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD_DIR)
