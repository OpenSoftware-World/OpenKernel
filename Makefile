# Tools
AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld

# Folders
BUILD = build
ISO = iso

# Kernel
KERNEL = $(BUILD)/OpenKernel.bin
ISOFILE = OpenKernel_1.0.iso

# Compile Tools
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -ISystemLib/Std -IDrivers/Vga
LDFLAGS = -m elf_i386 -T link.ld

# Source Files
ASM_SRC = Boot/boot.asm Boot/Mboot_desc/mboot.asm
C_SRC = main.c Drivers/Vga/vga.c SystemLib/SysCalls/basic_syscall.c SystemLib/TimeMng/time.c Kernel/KernelServices/SystemManagement/sysmng.c Kernel/KernelServices/Shell/shell.c SystemLib/Std/std.c Drivers/Keyboard/keyboard.c

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
	echo 'menuentry " OpenKernel 0.1" {' > $(ISO)/boot/grub/grub.cfg
	echo '    multiboot /boot/OpenKernel.bin' >> $(ISO)/boot/grub/grub.cfg
	echo '    boot' >> $(ISO)/boot/grub/grub.cfg
	echo '}' >> $(ISO)/boot/grub/grub.cfg
	echo 'menuentry " UEFI Settings" {' >> $(ISO)/boot/grub/grub.cfg
	echo '    fwsetup' >> $(ISO)/boot/grub/grub.cfg
	echo '	   echo "This option requires EFI mode."' >> $(ISO)/boot/grub/grub.cfg
	echo '}' >> $(ISO)/boot/grub/grub.cfg
	echo 'menuentry " About OpenKernel 0.1" {' >> $(ISO)/boot/grub/grub.cfg
	echo '    echo "OpenKernel 0.1 - Experimental OS by OpenSoftware-World"' >> $(ISO)/boot/grub/grub.cfg
	echo '    echo "This is a minimal kernel environment with shell and drivers."' >> $(ISO)/boot/grub/grub.cfg
	echo '    echo "Have fun exploring!"' >> $(ISO)/boot/grub/grub.cfg
	echo '    sleep 5' >> $(ISO)/boot/grub/grub.cfg
	echo '}' >> $(ISO)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISOFILE) $(ISO)

# Run
run: iso
	qemu-system-i386 -cdrom $(ISOFILE)

# Clean
clean:
	rm -rf $(BUILD) $(ISO) $(ISOFILE)