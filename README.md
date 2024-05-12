# LDD-BBB
Exploring device driver with Beaglebone Black.

## Building
In order to build the project. 

### Installing arm toolchain
The arm toolchain must be intalled. It can be found [here](https://snapshots.linaro.org/gnu-toolchain/) 

### Downloading Linux kernel
The linux kernel for beaglebone can be downloaded from [here](https://github.com/beagleboard/linux).
Check out the correct version as used for the beaglebone board.

### Compile kernel
Configure with beaglebone defconfig
```bash
make ARCH=arm bb.org_defconfig
```

Then compile the kernel
```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- uImage dtbs LOADADDR=0x80008000 -j4
```
Compile modules
```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- uImage modules -j4
```
### Compile the project
Generate compile_commands.json to navigate the code easily
```bash
sudo apt install bear
```
Then compile the project
```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export KERNELDIR=PATH_TO_KERNEL_DIR

cd LDD-BBB
bear -- make -j4
```