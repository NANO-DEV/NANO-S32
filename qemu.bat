"c:\Program Files\qemu\qemu-system-i386" -drive file=images\os-fd.img,if=floppy,media=disk,format=raw -drive file=images\os-hd.img,media=disk,format=raw -boot menu=on -serial mon:stdio -m 2 -vga std -netdev tap,id=u1,ifname=tap -device ne2k_pci,netdev=u1 -monitor vc -object filter-dump,id=f1,netdev=u1,file=dump.dat
