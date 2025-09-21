CROSS_COMPILE = riscv64-unknown-elf-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS = -Wall -O2 -ffreestanding -nostdlib -nostartfiles -mcmodel=medany -I./kernal -I./lib/libnet -I./lib/libc -I./lib/libhydra
ASFLAGS = -mcmodel=medany
LDFLAGS = -T boot/linker.ld

# Auto-detect source files
KERNEL_C_SOURCES = $(wildcard kernal/*.c)
KERNEL_S_SOURCES = $(wildcard kernal/*.s)
LIBC_C_SOURCES = $(wildcard lib/libc/*.c)
LIBNET_C_SOURCES = $(wildcard lib/libnet/*.c)
LIBHYDRA_C_SOURCES = $(wildcard lib/libhydra/*.c)
SERVER_C_SOURCES = $(wildcard server/*.c)
APPS_C_SOURCES = $(wildcard apps/*.c) $(wildcard apps/raytracer/*.c)

# Convert sources to object files
KERNEL_C_OBJS = $(KERNEL_C_SOURCES:.c=.o)
KERNEL_S_OBJS = $(KERNEL_S_SOURCES:.s=.o)
LIBC_OBJS = $(LIBC_C_SOURCES:.c=.o)
LIBNET_OBJS = $(LIBNET_C_SOURCES:.c=.o)
LIBHYDRA_OBJS = $(LIBHYDRA_C_SOURCES:.c=.o)
SERVER_OBJS = $(SERVER_C_SOURCES:.c=.o)
APPS_OBJS = $(APPS_C_SOURCES:.c=.o)

# All object files
OBJS = boot/start.o \
       $(KERNEL_C_OBJS) $(KERNEL_S_OBJS) \
       $(LIBC_OBJS) $(LIBNET_OBJS) $(LIBHYDRA_OBJS) \
       $(SERVER_OBJS) $(APPS_OBJS)

all: kernel.elf

boot/start.o: boot/start.s
	$(CC) $(ASFLAGS) -c $< -o $@

kernal/%.o: kernal/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernal/%.o: kernal/%.s
	$(CC) $(ASFLAGS) -c $< -o $@

lib/libnet/%.o: lib/libnet/%.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libc/%.o: lib/libc/%.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libnet/%.o: lib/libnet/%.c
	$(CC) $(CFLAGS) -c $< -o $@

lib/libhydra/%.o: lib/libhydra/%.c
	$(CC) $(CFLAGS) -c $< -o $@

server/%.o: server/%.c
	$(CC) $(CFLAGS) -c $< -o $@

apps/%.o: apps/%.c
	$(CC) $(CFLAGS) -c $< -o $@

apps/raytracer/%.o: apps/raytracer/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJS)
	@echo "Linking kernel..."
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Build complete: kernel.elf"

# Debug target to show detected files
debug-files:
	@echo "Detected C sources:"
	@echo "  Kernel: $(KERNEL_C_SOURCES)"
	@echo "  LibC: $(LIBC_C_SOURCES)"
	@echo "  LibNet: $(LIBNET_C_SOURCES)"
	@echo "  LibHydra: $(LIBHYDRA_C_SOURCES)"
	@echo "  Server: $(SERVER_C_SOURCES)"
	@echo "  Apps: $(APPS_C_SOURCES)"
	@echo "Detected S sources:"
	@echo "  Kernel: $(KERNEL_S_SOURCES)"
	@echo "All objects: $(OBJS)"

clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OBJS) kernel.elf
	@echo "Clean complete"

# QEMU run targets
run: kernel.elf
	@echo "Starting QEMU..."
	qemu-system-riscv64 -machine virt -nographic -kernel $<

run-debug: kernel.elf
	@echo "Starting QEMU with GDB server..."
	qemu-system-riscv64 -machine virt -nographic -kernel $< -s -S

# Network-enabled QEMU (for testing network features)
run-net: kernel.elf
	@echo "Starting QEMU with network..."
	qemu-system-riscv64 -machine virt -nographic -kernel $< \
		-netdev user,id=net0 \
		-device virtio-net-device,netdev=net0

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build kernel.elf (default)"
	@echo "  clean        - Remove build artifacts"
	@echo "  run          - Run kernel in QEMU"
	@echo "  run-debug    - Run kernel in QEMU with GDB server"
	@echo "  run-net      - Run kernel in QEMU with network"
	@echo "  debug-files  - Show detected source files"
	@echo "  help         - Show this help"

.PHONY: all clean run run-debug run-net debug-files help
