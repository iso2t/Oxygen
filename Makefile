# Oxygen kernel build.
#
# Run from a WSL2 (Ubuntu/Debian) shell - this Makefile uses POSIX `find`.
#
# One-time setup inside WSL2:
#   sudo apt update
#   sudo apt install -y build-essential xorriso grub-pc-bin grub-common \
#                       qemu-system-x86 mtools
#
# Toolchain: by default we use Ubuntu's host gcc/ld (works fine for an
# x86_64 kernel on an x86_64 host because we build freestanding). To use
# a dedicated cross-compiler later, build x86_64-elf-gcc per the OSDev
# wiki "GCC Cross-Compiler" recipe, put it on PATH, then build with:
#   make CROSS=x86_64-elf-
#
# Targets:
#   make            - build kernel ELF                 (build/oxygen.elf)
#   make iso        - build bootable ISO image         (build/oxygen.iso)
#   make run        - boot the ISO in QEMU
#   make clean      - remove build artifacts

ARCH        ?= x86_64
CROSS       ?=

CC          := $(CROSS)gcc
LD          := $(CROSS)ld

KERNEL_DIR  := kernel
BUILD_DIR   := build
ISO_DIR     := $(BUILD_DIR)/iso

CFLAGS := -std=c11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie     \
          -fno-tree-loop-distribute-patterns                                  \
          -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mno-80387                \
          -Wall -Wextra -Werror -O2 -g                                        \
          -I$(KERNEL_DIR)/include

LDFLAGS := -nostdlib -z max-page-size=0x1000 \
           -T $(KERNEL_DIR)/arch/$(ARCH)/linker.ld

C_SOURCES := $(shell find $(KERNEL_DIR) -name '*.c')
S_SOURCES := $(shell find $(KERNEL_DIR) -name '*.S')

C_OBJECTS := $(C_SOURCES:%.c=$(BUILD_DIR)/%.o)
S_OBJECTS := $(S_SOURCES:%.S=$(BUILD_DIR)/%.o)
OBJECTS   := $(C_OBJECTS) $(S_OBJECTS)

KERNEL_BIN := $(BUILD_DIR)/oxygen.elf
ISO        := $(BUILD_DIR)/oxygen.iso

.PHONY: all kernel iso run clean

all: $(KERNEL_BIN)

kernel: $(KERNEL_BIN)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJECTS) $(KERNEL_DIR)/arch/$(ARCH)/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

iso: $(ISO)

$(ISO): $(KERNEL_BIN) boot/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/oxygen.elf
	cp boot/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR) 2>/dev/null

run: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO) -serial stdio -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD_DIR)
