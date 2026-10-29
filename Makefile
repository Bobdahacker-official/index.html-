CC = gcc
NASM = nasm
LD = ld
OBJCOPY = objcopy
XORRISO = xorriso
CFLAGS = -m32 -std=gnu11 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -fno-asynchronous-unwind-tables -Os -Wall -Wextra -Werror

all: CookieOS.iso

build:
	mkdir -p build/iso
build/kernel.o: kernel.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/entry.o: entry.asm | build
	$(NASM) -f elf32 $< -o $@
build/kernel.elf: build/entry.o build/kernel.o linker.ld
	$(LD) -m elf_i386 -T linker.ld -o $@ build/entry.o build/kernel.o
build/kernel.bin: build/kernel.elf
	$(OBJCOPY) -O binary $< $@
build/boot.bin: boot.asm build/kernel.bin
	$(NASM) -f bin -DKSECTORS=$$((($$(wc -c < build/kernel.bin) + 511) / 512)) $< -o $@
build/iso/boot.img: build/boot.bin build/kernel.bin
	cat build/boot.bin build/kernel.bin > $@
	truncate -s 1474560 $@
CookieOS.iso: build/iso/boot.img
	$(XORRISO) -as mkisofs -quiet -V COOKIEOS -b boot.img -o $@ build/iso
clean:
	rm -rf build CookieOS.iso
.PHONY: all clean
