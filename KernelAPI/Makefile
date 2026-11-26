# Tools
# It stores the names of the tools required to compile the OpenKernel Kernel API along with the corresponding variable names.
# Unless you plan to use a different tool for compiling or have modified the code to be compatible with the compiler, do not change this section under any circumstances.
CC = i686-elf-gcc
AR = i686-elf-ar

# Folders
# Specifies the folder where the object files will be built and stored.
# It is completely safe to edit this section.
BUILD = build

# Library
# Specifies the file name under which the Kernel API objects will be merged into a static library.
# It is completely safe to edit this section.
LIB = $(BUILD)/libopenkernel_api.a

# Compile Tools
# Refers to compilation options.
# Unless you have a specific need, please do not change this.
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -I .
# Extra include root that resolves the <OpenKernel/...> style headers.
# It is kept outside CFLAGS on purpose: PRINCIPLES.md forbids modifying CFLAGS.
INCLUDES = -I ../..

# Source Files
# Refers to the source files of the Kernel API.
# You can modify this section as long as you customize OpenKernel to suit your needs. (Don't forget to include the code files you've added here as well.)
C_SRC = nonuniversal_api_defs.c \
	    OpenKernelUniversalAPI/universal_api_defs.c

# Obj
# Specifies where the compiled files will be stored. (Unless you plan to redesign the system from scratch, please do not change this setting.)
C_OBJ = $(patsubst %.c,$(BUILD)/%.o,$(C_SRC))

OBJS = $(C_OBJ)

all: $(LIB)

# Build Folders
# Creates the folder in the BUILD variable. (Nothing to change.)
$(BUILD):
	mkdir -p $(BUILD)

# Compile C
# It compiles the C files and places them in the corresponding BUILD folder. (You don't need to change this unless you're using a different compiler.)
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Library link
# It merges the compiled objects into a single static library. (You don't need to change this unless you're planning to implement a different design.)
$(LIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)

# Clean
# Cleans up leftover files. (You can change this part if you want.)
clean:
	rm -rf $(BUILD)
