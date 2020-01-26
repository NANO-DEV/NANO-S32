@echo off
:: Call with -tap to use tap network device instead of default

if /i "%~1"=="-tap" goto tap
@echo on
"c:\Program Files\qemu\qemu-system-i386" ^
-drive file=images\os-fd.img,if=floppy,media=disk,format=raw ^
-drive file=images\os-hd.img,media=disk,format=raw ^
-boot c,menu=on -serial mon:stdio -m 2 -vga std ^
-netdev user,id=u1,net=192.168.2.0/24,dhcpstart=192.168.2.15,hostfwd=udp::8086-:8086 ^
-device ne2k_pci,netdev=u1 -monitor vc -soundhw sb16 ^
-object filter-dump,id=f1,netdev=u1,file=dump.dat
@echo off
goto end
:tap
@echo on
"c:\Program Files\qemu\qemu-system-i386" ^
-drive file=images\os-fd.img,if=floppy,media=disk,format=raw ^
-drive file=images\os-hd.img,media=disk,format=raw ^
-boot c,menu=on -serial mon:stdio -m 2 -vga std ^
-netdev tap,id=u1,ifname=tap -device ne2k_pci,netdev=u1 ^
-monitor vc -soundhw sb16 ^
-object filter-dump,id=f1,netdev=u1,file=dump.dat
@echo off
goto end
:end
