# Tools
# It stores the names of the tools required to compile OpenKernel or your OpenKernel-based system along with the corresponding variable names.
# Unless you plan to use a different tool for compiling or have modified the code to be compatible with the compiler, do not change this section under any circumstances.
AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld

# Folders
# Specifies the folders where the .iso file will be built and stored.
# It is completely safe to edit this section.
BUILD = build
ISO = iso

# Kernel
# Specifies the file name under which the kernel will be merged and the .iso file in which the system will be contained.
# It is completely safe to edit this section.
KERNEL = $(BUILD)/os.bin
ISOFILE = your_os.iso

# Compile Tools
# Refers to compilation options.
# Unless you have a specific need, please do not change this.
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -I .
LDFLAGS = -m elf_i386 -T link.ld

# Source Files
# Refers to the source files of OpenKernel and your OpenKernel-based operating system.
# You can modify this section as long as you customize OpenKernel to suit your needs. (Don’t forget to include the code files you’ve added here as well.)
ASM_SRC = # Bootloader and assembly files (You must specify the file paths.)
C_SRC = OpenKernel/Drivers/Vga/vga.c \
	    OpenKernel/Drivers/Cpu/cpu.c \
		OpenKernel/OFS/ofs.c \
		OpenKernel/Drivers/Ahci/ahci.c \
		OpenKernel/Drivers/Ata/ata.c \
		OpenKernel/SystemLib/Memory/mem.c \
		OpenKernel/Drivers/Mouse/mouse.c \
		OpenKernel/SystemLib/KernelFunc/kernel_func.c \
	    OpenKernel/SystemLib/SysCalls/syscall.c \
		OpenKernel/SystemLib/TimeMng/time.c \
		OpenKernel/Kernel/KernelServices/SystemManagement/sysmng.c \
		OpenKernel/SystemLib/Std/std.c \
		OpenKernel/Drivers/Sound/sb16.c \
		OpenKernel/Drivers/Keyboard/keyboard.c \
		OpenKernel/Drivers/Serial/serial.c \
		OpenKernel/FAT12/fat12.c \
		OpenKernel/FAT16/fat16.c \
		OpenKernel/FAT32/fat32.c \
		OpenKernel/VFS/vfs.c \
		OpenKernel/Drivers/PCIBus/pci.c \
		OpenKernel/Drivers/Network/Ethernet/ethernet.c \
		OpenKernel/Drivers/Network/e1000/e1000.c \
		OpenKernel/Drivers/Network/rtl8139/rtl8139.c \
		OpenKernel/Kernel/ErrorHandler/panic.c \
		OpenKernel/Kernel/KernelServices/LogService/log.c \
		OpenKernel/Kernel/KernelServices/VirtualMem/vmem.c \
		OpenKernel/Kernel/KernelServices/SecurityChecker/security.c \
		OpenKernel/SystemLib/TaskMng/task.c \
		OpenKernel/SystemLib/TypeConversion/type_conv.c

# Obj
# Specifies where the compiled files will be stored. (Unless you plan to redesign the system from scratch, please do not change this setting.)
ASM_OBJ = $(ASM_SRC:%.asm=$(BUILD)/%.o)
C_OBJ = $(C_SRC:%.c=$(BUILD)/%.o)

OBJS = $(ASM_OBJ) $(C_OBJ)

all: $(KERNEL)

# Build Folders
# Creates the folder in the BUILD variable. (Nothing to change.)
$(BUILD):
	mkdir -p $(BUILD)

# Compile ASM
# It compiles the assembly files and places them in the corresponding BUILD folder. (You do not need to change this unless you are using a different compiler
$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

# Compile C
# It compiles the C files and places them in the corresponding BUILD folder. (You don't need to change this unless you're using a different compiler.)
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Kernel link
# It links the kernel using the link.ld file. (You don't need to change this unless you're planning to implement a different design.)
$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(KERNEL)

# ISO Folder Structure
# It creates a file with the .iso extension. (You can safely make your changes here.)
iso: $(KERNEL)
	mkdir -p $(ISO)/boot/grub
	cp $(KERNEL) $(ISO)/boot/
	cp Boot/grub/grub.cfg $(ISO)/boot/grub/
	grub-mkrescue -o $(ISOFILE) $(ISO)
	test -f disk.img || qemu-img create disk.img 25M

# Run
# This refers to the execution section. (If you're familiar with QEMU parameters, you can modify this section.)
run: iso
	qemu-system-i386 \
	-m 512M \
	-smp 4 \
	-cdrom $(ISOFILE) \
	-drive file=disk.img,format=raw,if=ide \
	-audiodev pa,id=snd0 \
	-device sb16,audiodev=snd0 \
	-boot d \
	-serial stdio

# Clean
# Cleans up leftover files. (You can change this part if you want.)
clean:
	rm -rf $(BUILD) $(ISO) $(ISOFILE)
