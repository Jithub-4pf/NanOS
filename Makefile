# Cross-compiler tools
CC=i686-elf-gcc
AS=i686-elf-as
LD=i686-elf-ld

# Directories
SRC_DIR=src
INC_DIR=include
BOOT_DIR=boot

# Kernel source files
KERNEL_SRC=$(SRC_DIR)/kernel.c
KERNEL_OBJ=$(SRC_DIR)/kernel.o
IDT_SRC=$(SRC_DIR)/idt.c $(BOOT_DIR)/idt_asm.s
IDT_OBJ=$(SRC_DIR)/idt.o $(BOOT_DIR)/idt_asm.o
ISR_SRC=$(SRC_DIR)/monitor.c $(SRC_DIR)/gdt.c $(SRC_DIR)/pic.c $(SRC_DIR)/keyboard.c $(SRC_DIR)/ui.c $(SRC_DIR)/heap.c $(SRC_DIR)/paging.c $(SRC_DIR)/physmem.c $(SRC_DIR)/process.c $(SRC_DIR)/sched.c $(SRC_DIR)/context_switch.s $(SRC_DIR)/blockdev.c $(SRC_DIR)/ramdisk.c $(SRC_DIR)/ext2.c $(SRC_DIR)/vfs.c $(SRC_DIR)/string.c
ISR_OBJ=$(SRC_DIR)/monitor.o $(SRC_DIR)/gdt.o $(SRC_DIR)/pic.o $(SRC_DIR)/keyboard.o $(SRC_DIR)/ui.o $(SRC_DIR)/heap.o $(SRC_DIR)/paging.o $(SRC_DIR)/physmem.o $(SRC_DIR)/process.o $(SRC_DIR)/sched.o $(SRC_DIR)/context_switch.o $(SRC_DIR)/blockdev.o $(SRC_DIR)/ramdisk.o $(SRC_DIR)/ext2.o $(SRC_DIR)/vfs.o $(SRC_DIR)/string.o

# Assembly source files
ASM_SRC=$(BOOT_DIR)/boot.s
ASM_OBJ=$(BOOT_DIR)/boot.o

# Linker script
LINKER_SCRIPT=linker.ld

# Output file
KERNEL_BIN=myos.bin

# Build the kernel
all: $(KERNEL_BIN)
	echo "Kernel built successfully"

$(KERNEL_BIN): $(ASM_OBJ) $(KERNEL_OBJ) $(IDT_OBJ) $(ISR_OBJ)
	$(LD) -T $(LINKER_SCRIPT) -o $@ $(ASM_OBJ) $(KERNEL_OBJ) $(IDT_OBJ) $(ISR_OBJ)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -ffreestanding -I$(INC_DIR) -c $< -o $@

$(BOOT_DIR)/%.o: $(BOOT_DIR)/%.s
	$(AS) $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o $(BOOT_DIR)/*.o $(KERNEL_BIN)

run: $(KERNEL_BIN)
	qemu-system-i386 -kernel $(KERNEL_BIN) -m 32M

.PHONY: all clean run
