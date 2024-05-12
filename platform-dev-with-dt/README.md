# Experiment with device tree overlay 
These steps are used to create pseudo devices
- Put am335x-boneblack-psd.dtsi in arch/arm/boot/dts
- Modify am335-bone-black to include am335x-boneblack-psd.dtsi
- Build the dtb
```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

make am335x-boneblack.dtb
```
- Then the dtb file can be loaded on the target.