# NANO S32

NANO S32 is a small operating system for 32-bit x86 architectures which operates in protected mode.

## Clone this repository in a local machine
```
(sudo apt-get install git)
git clone https://github.com/NANO-DEV/NANO-S32.git
```
## Update an existing local copy of this repository
```
git pull
```
## Install required building tools
```
sudo apt-get install gcc nasm make
(sudo apt-get install qemu)
```
## Build
```
(from the root directory)
make
```
## Test using qemu
```
(from the root directory)
make qemu
```
