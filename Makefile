# Makefile

# Directories
IMAGEDIR := images/
SOURCEDIR := source/
FSTOOLSDIR := fstools/

# User files and args for mkfs
USERFILES := $(SOURCEDIR)programs/test.bin $(SOURCEDIR)programs/edit.bin \
	$(SOURCEDIR)programs/nas.bin $(SOURCEDIR)programs/sample.s \
	$(SOURCEDIR)programs/unet.bin
MKFSARGS := $(SOURCEDIR)boot/boot.bin $(SOURCEDIR)kernel.n32 $(USERFILES)

# Make source and create images
all: $(FSTOOLSDIR)mkfs
	$(MAKE) $@ -C $(SOURCEDIR) --no-print-directory
	mkdir -p $(IMAGEDIR)
	$(FSTOOLSDIR)mkfs $(IMAGEDIR)os-fd.img 2880 $(MKFSARGS)
	$(FSTOOLSDIR)mkfs $(IMAGEDIR)os-hd.img 28800 $(MKFSARGS)

# mkfs generates disk images
$(FSTOOLSDIR)mkfs: $(FSTOOLSDIR)mkfs.c $(SOURCEDIR)fs.h
	gcc -Werror -Wall -I$(SOURCEDIR) -o $(FSTOOLSDIR)mkfs $(FSTOOLSDIR)mkfs.c

# Generate tags
ctags:
	ctags -R

# run in emulators
# Specify QEMU path here
QEMU = qemu-system-i386

QEMUOPTS = -drive file=$(IMAGEDIR)os-fd.img,if=floppy,media=disk,format=raw \
	-drive file=$(IMAGEDIR)os-hd.img,media=disk,format=raw -d guest_errors \
	-boot menu=on -serial mon:stdio -m 2 -vga std -monitor vc \
	-netdev user,id=u1,net=192.168.2.0/24,dhcpstart=192.168.2.15,hostfwd=udp::8086-:8086 \
	-device ne2k_pci,netdev=u1 -object filter-dump,id=f1,netdev=u1,file=dump.dat

qemu: all
	$(QEMU) $(QEMUOPTS)

# Clean
clean:
	rm -f $(FSTOOLSDIR)mkfs
	rm -f tags
	rm -f $(IMAGEDIR)os-fd.img $(IMAGEDIR)os-hd.img
	$(MAKE) $@ -C $(SOURCEDIR) --no-print-directory
	@find . -name "*.dat" -type f -delete

.PHONY: all ctags qemu clean
