CROSS_COMPILE = riscv64-unknown-elf-
.DEFAULT_GOAL := all
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# Historical comparison labels: direct supervisor call (KERNEL) versus
# supervisor wrapper/dispatch (USERSPACE), both using the same primitives.
# The local comparison does not measure a hardware privilege transition.
MIGRATION_MODE ?= USERSPACE

ifeq ($(MIGRATION_MODE),KERNEL)
  MIG_CFLAGS = -DMIGRATION_KERNEL_RESIDENT=1
else ifeq ($(MIGRATION_MODE),USERSPACE)
  MIG_CFLAGS = -DMIGRATION_USERSPACE=1
else
  $(error MIGRATION_MODE must be KERNEL or USERSPACE, got '$(MIGRATION_MODE)')
endif

# Per-packet / per-syscall tracing. Off by default: when on it emits
# ~3,300 lines/s (~250 KB/s) per node, which fills the disk on long runs.
# Turn on only to debug the packet path: make TRACE_PACKETS=1
TRACE_PACKETS ?= 0

# Scheduler switch/yield tracing. Also off by default — it is the largest
# log source once packet tracing is disabled. Does not affect BENCH output.
TRACE_SCHED ?= 0

# Memory primitive implementation used for controlled A/B benchmarking.
# WORD is the optimized production default; BYTE preserves the original
# byte-at-a-time loops as a reproducible baseline.
MEMOPS_MODE ?= WORD

# Null-syscall / IPC round-trip micro-benchmarks used for the Amoeba
# ratio-normalised comparison. Off by default because it spawns extra
# tasks during boot: make MICRO_BENCH=1
MICRO_BENCH ?= 0

# Per-message IPC tracing. On by default so existing logs are unchanged;
# set TRACE_IPC=0 when benchmarking the IPC path, since each trace is a
# synchronous UART write that dominates the measurement.
TRACE_IPC ?= 1

ifeq ($(MEMOPS_MODE),WORD)
  MEMOPS_CFLAGS = -DMEMOPS_WORD=1
else ifeq ($(MEMOPS_MODE),BYTE)
  MEMOPS_CFLAGS = -DMEMOPS_BYTE=1
else
  $(error MEMOPS_MODE must be WORD or BYTE, got '$(MEMOPS_MODE)')
endif

# Make does not normally notice when a command-line build option changes.
# Keep a small configuration stamp so switching migration/trace modes cannot
# silently reuse objects compiled with the previous configuration.
BUILD_DIR := build
KERNEL_IMAGE := $(BUILD_DIR)/kernel.elf
CONFIG_STAMP := $(BUILD_DIR)/.build-config

CFLAGS = -Wall -O2 -ffreestanding -nostdlib -nostartfiles -mcmodel=medany -MMD -MP \
         -I./kernel -I./lib/libc -I./lib/libhydra $(MIG_CFLAGS) \
         $(MEMOPS_CFLAGS) -DTRACE_PACKETS=$(TRACE_PACKETS) -DTRACE_SCHED=$(TRACE_SCHED) \
         -DTRACE_IPC=$(TRACE_IPC) \
         $(if $(filter-out 0,$(MICRO_BENCH)),-DMICRO_BENCH=1,)
ASFLAGS = -mcmodel=medany
LDFLAGS = -T boot/linker.ld

# Auto-detect source files
KERNEL_C_SOURCES = $(wildcard kernel/*.c)
KERNEL_S_SOURCES = $(wildcard kernel/*.s)
LIBC_C_SOURCES = $(wildcard lib/libc/*.c)
LIBHYDRA_C_SOURCES = $(wildcard lib/libhydra/*.c)
APPS_C_SOURCES = $(wildcard apps/*.c)

# Convert sources to object files
KERNEL_C_OBJS = $(KERNEL_C_SOURCES:.c=.o)
KERNEL_S_OBJS = $(KERNEL_S_SOURCES:.s=.o)
LIBC_OBJS = $(LIBC_C_SOURCES:.c=.o)
LIBHYDRA_OBJS = $(LIBHYDRA_C_SOURCES:.c=.o)
APPS_OBJS = $(APPS_C_SOURCES:.c=.o)

# All object files
OBJS = $(addprefix $(BUILD_DIR)/,boot/start.o \
       $(KERNEL_C_OBJS) $(KERNEL_S_OBJS) \
       $(LIBC_OBJS) $(LIBHYDRA_OBJS) $(APPS_OBJS))

DEPS = $(OBJS:.o=.d)
-include $(DEPS)

$(BUILD_DIR):
	mkdir -p $@

$(CONFIG_STAMP): FORCE | $(BUILD_DIR)
	@{ \
	  echo "MIGRATION_MODE=$(MIGRATION_MODE)"; \
	  echo "TRACE_PACKETS=$(TRACE_PACKETS)"; \
	  echo "TRACE_SCHED=$(TRACE_SCHED)"; \
	  echo "MEMOPS_MODE=$(MEMOPS_MODE)"; \
	  echo "MICRO_BENCH=$(MICRO_BENCH)"; \
	  echo "TRACE_IPC=$(TRACE_IPC)"; \
	} > $(CONFIG_STAMP).tmp
	@if ! cmp -s $(CONFIG_STAMP).tmp $(CONFIG_STAMP); then \
	  mv $(CONFIG_STAMP).tmp $(CONFIG_STAMP); \
	  echo "Build configuration changed; recompiling objects"; \
	else \
	  rm -f $(CONFIG_STAMP).tmp; \
	fi

$(OBJS): $(CONFIG_STAMP)

all: $(KERNEL_IMAGE)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	mkdir -p $(@D)
	$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL_IMAGE): $(OBJS)
	@echo "Linking kernel..."
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Build complete: $@"

# Debug target to show detected files
debug-files:
	@echo "Detected C sources:"
	@echo "  Kernel: $(KERNEL_C_SOURCES)"
	@echo "  LibC: $(LIBC_C_SOURCES)"
	@echo "  LibHydra: $(LIBHYDRA_C_SOURCES)"
	@echo "  Apps: $(APPS_C_SOURCES)"
	@echo "Detected S sources:"
	@echo "  Kernel: $(KERNEL_S_SOURCES)"
	@echo "All objects: $(OBJS)"

clean:
	@echo "Cleaning build artifacts..."
	rm -rf -- build
	@echo "Clean complete"

# QEMU run targets
run: $(KERNEL_IMAGE)
	@echo "Starting QEMU..."
	qemu-system-riscv64 -machine virt -nographic -kernel $<

run-debug: $(KERNEL_IMAGE)
	@echo "Starting QEMU with GDB server..."
	qemu-system-riscv64 -machine virt -nographic -kernel $< -s -S

# Network-enabled QEMU (for testing network features)
run-net: $(KERNEL_IMAGE)
	@echo "Starting QEMU with network..."
	qemu-system-riscv64 -machine virt -nographic -kernel $< \
		-global virtio-mmio.force-legacy=false \
		-netdev user,id=net0 \
		-device virtio-net-device,netdev=net0

# Bounded two-node migration acceptance test
run-distributed: $(KERNEL_IMAGE)
	@echo "Starting distributed system test..."
	./bench/two_node_test.sh

# Help target
help:
	@echo "Available targets:"
	@echo "  all              - Build build/kernel.elf (default)"
	@echo "  clean            - Remove build artifacts"
	@echo "  run              - Run kernel in QEMU"
	@echo "  run-debug        - Run kernel in QEMU with GDB server"
	@echo "  run-net          - Run kernel in QEMU with network"
	@echo "  run-distributed  - Run bounded two-node migration acceptance (60s)"
	@echo "  debug-files      - Show detected source files"
	@echo "  help             - Show this help"
	@echo ""
	@echo "Build configuration:"
	@echo "  MIGRATION_MODE   - KERNEL or USERSPACE (currently: $(MIGRATION_MODE))"
	@echo "                     Examples:"
	@echo "                       make MIGRATION_MODE=KERNEL"
	@echo "                       make MIGRATION_MODE=USERSPACE"
	@echo "  TRACE_PACKETS    - 0 or 1 (currently: $(TRACE_PACKETS))"
	@echo "                     Per-packet syscall tracing. Off by default;"
	@echo "                     on it writes ~250 KB/s of logs per node."
	@echo "                     Example: make TRACE_PACKETS=1"
	@echo "  TRACE_SCHED      - 0 or 1 (currently: $(TRACE_SCHED))"
	@echo "                     Scheduler switch/yield tracing. Off by default."
	@echo "                     Does not affect BENCH benchmark output."
	@echo "                     Example: make TRACE_SCHED=1"
	@echo "  MEMOPS_MODE      - WORD or BYTE (currently: $(MEMOPS_MODE))"
	@echo "                     WORD is optimized; BYTE is the A/B baseline."

.PHONY: all clean run run-debug run-net run-distributed debug-files help FORCE

FORCE:
