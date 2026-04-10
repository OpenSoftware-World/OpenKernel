# Tools
AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld

# Folders
BUILD = build
ISO = iso

# Kernel
KERNEL = $(BUILD)/openkernel.bin
ISOFILE = openkernel.iso

# Compile Tools
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -ISystemLib/Std -IDrivers/Vga
LDFLAGS = -m elf_i386 -T link.ld

# Source Files
ASM_SRC = Boot/boot.asm Boot/Mboot_desc/mboot.asm
C_SRC = main.c \
		Drivers/Vga/vga.c \
		Drivers/Cpu/cpu.c \
		OFS/ofs.c \
		Drivers/Ata/ata.c \
		SystemLib/Memory/mem.c \
	    SystemLib/SysCalls/basic_syscall.c \
		SystemLib/TimeMng/time.c \
		Kernel/KernelServices/SystemManagement/sysmng.c \
		Kernel/KernelServices/Shell/shell.c \
		SystemLib/Std/std.c \
		Drivers/Keyboard/keyboard.c

# Obj
ASM_OBJ = $(ASM_SRC:%.asm=$(BUILD)/%.o)
C_OBJ = $(C_SRC:%.c=$(BUILD)/%.o)

OBJS = $(ASM_OBJ) $(C_OBJ)

all: $(KERNEL)

# Build Folders
$(BUILD):
	mkdir -p $(BUILD)

# Compile ASM
$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

# Compile C
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Kernel link
$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(KERNEL)

# ISO Folder Structure
iso: $(KERNEL)
	mkdir -p $(ISO)/boot/grub
	cp $(KERNEL) $(ISO)/boot/
	cp Boot/grub/grub.cfg $(ISO)/boot/grub/
	grub-mkrescue -o $(ISOFILE) $(ISO)
	qemu-img create disk.img 20M

# Run
run: iso
	qemu-system-i386 \
	-cdrom $(ISOFILE) \
	-drive file=disk.img,format=raw,if=ide \
	-boot d

# Clean
clean:
	rm -rf $(BUILD) $(ISO) $(ISOFILE)