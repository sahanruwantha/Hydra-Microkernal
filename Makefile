CC=gcc
LD=ld

CFLAGS=-m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic
LDFLAGS=-T linker.ld -nostdlib -m elf_i386

KERNEL_OBJ=kernel.o
ISO_DIR=isodir
BOOT_DIR=$(ISO_DIR)/boot
GRUB_DIR=$(BOOT_DIR)/grub

all: os-image

build: os-image

$(KERNEL_OBJ): kernel/kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

os-image: kernel.bin
	mkdir -p $(GRUB_DIR)
	cp grub/grub.cfg $(GRUB_DIR)
	cp kernel.bin $(BOOT_DIR)/kernel
	grub-mkrescue -o PakayaOs.iso $(ISO_DIR)

run: os-image
	qemu-system-i386 -boot d -cdrom PakayaOs.iso -m 512M -no-reboot -no-shutdown

clean:
	rm -rf *.o *.bin *.iso isodir
