# Tools
AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld

# Folders
BUILD = build
ISO = iso

# Kernel
KERNEL = $(BUILD)/os.bin
ISOFILE = your_os.iso

# Compile Tools
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -Werror -fstack-protector -I .
LDFLAGS = -m elf_i386 -T link.ld

# Source Files
ASM_SRC = # Bootloader and assembly files
C_SRC = OpenKernel/Drivers/Vga/vga.c \
	    OpenKernel/Drivers/Cpu/cpu.c \
		OpenKernel/OFS/ofs.c \
		OpenKernel/Drivers/AHCI/ahci.c \
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
		OpenKernel/Kernel/KernelServices/SecurityChecker/securchk.c \
		OpenKernel/SystemLib/TaskMng/task.c \
		OpenKernel/SystemLib/TypeConversion/type_conv.c

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
	test -f disk.img || qemu-img create disk.img 25M

# Run
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
clean:
	rm -rf $(BUILD) $(ISO) $(ISOFILE)
