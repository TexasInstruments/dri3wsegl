# DRI3 WSEGL

DRI3WSEGL is a plugin library for Imagination's PVR driver for SGX GPUs on Texas Instrument's SoCs. DRI3WSEGL enables 3D acceleration with X11 using DRI3.

## Warning

DRI3WSEGL is still very much a work in progress. It also depends on other components which are works in progress. This README is not a step-by-step guide that you could follow blindly. You will most likely encounter issues if you try this out.

That said, I have succesfully ran kmscube from kms++ (https://github.com/tomba/kmsxx) and glmark2 using DRI3WSEGL, and I hope the issues and unclarities will be solved in the future.

Good bug reports and contributions are very much appreciated.

## Getting Started

This README mainly covers using DRI3WSEGL with Buildroot environment on OMAP5/AM572x/DRA7xx SoCs. DRI3WSEGL should also work on AM335x and AM437x, but it has not been tested.

## SGX Driver

The assumption is that you are already familiar with the SGX driver, and preferrably have it running on your system. These notes are not a full guide on SGX drivers, nor is this project about the SGX driver.

To use the SGX driver you need a kernel that's compatible with the SGX driver and the additional DT and hwmod changes for SGX. I have been using TI's v4.14 based kernel:

git://git.ti.com/ti-linux-kernel/ti-linux-kernel.git ti2018.04-rc3-int

### SGX Userspace Libraries (UM)

SGX userspace libraries are distributed as binary only. You can get the latest from:

git://git.ti.com/graphics/omap5-sgx-ddk-um-linux.git ti-img-sgx/1.14.3699939

The binaries for OMAP5/AM572x/DRA7xx are located under targetfs/jacinto6evm/

### SGX Kernel Driver (KM)

SGX kernel driver can be found from:

git://git.ti.com/graphics/omap5-sgx-ddk-linux.git ti-img-sgx/1.14.3699939/k4.14

You can find some instructions from the eurasia_km/README and eurasia_km/INSTALL files, but here's a script I have been using to compile the module:

```
export BUILD=release
export TARGET_PRODUCT=jacinto6evm
export ARCH=arm
export KERNELDIR=<path-to-my-kernel-tree>
export CROSS_COMPILE=<path-to-my-cross-compiler>/arm-linux-gnueabihf-
export DDKROOT=`pwd`

cd $DDKROOT/eurasia_km/eurasiacon/build/linux2/omap_linux
make -j4 $*
```

The kernel module will be eurasia_km/eurasiacon/binary2_omap_linux_release/target/kbuild/pvrsrvkm.ko

Note that the SGX kernel driver needs CONFIG_DRM_LEGACY to be enabled in the kernel.

## Building

To build and to use DRI3WSEGL you will need the following:

### X11 Libraries

You need the following X11 libraries:

x11-xcb, xcb, xcb-dri3, xcb-present

### GBM

libgbm is not needed when using Dumb buffers. However, applications often depend on it, and DRI3WSEGL can also use GBM buffers (see BO_TYPE variable below). A custom version of libgbm is needed when used with SGX. This libgbm can be found from:

git://git.ti.com/glsdk/libgbm.git test

Unfortunately the custom libgbm builds libgbm.so.2.0.0 in the master branch, instead of libgbm.so.1.0.0 as it should. The SGX userspace libraries have been linked against 2.0.0. In the above 'test' branch I have reverted the versioning change, but you will still need to create a symlink to create libgbm.so.2 for the SGX userspace libs.

```
ln -s libgbm.so.1.0.0 libgbm.so.2
```

### CMake variables

Variable           | Description                          | Values          | Default
-------------------|-------------------                   |-------------    | ---------------
CMAKE_BUILD_TYPE   | Release or Debug build for DRI3WSEGL | Release/Debug   | Release
PVR_BUILD_TYPE     | Build type of the SGX KM & UM        | Release/Debug   | Release
PVR_KM             | Path to SGX kernel driver directory  |                 |
PVR_UM_LIBS        | Path to SGX userspace libraries      |                 |
BO_TYPE            | Buffer type used by DRI3WSEGL        | Dumb/GBM        | Dumb
ENABLE_DRI3TEST    | Build dri3test tool                  | True/False      | False

## Using

To use DRI3WSEGL you need an X11 driver that supports DRI3. Unfortunately, as far as I know, there's none out there that would be suitable and of production quality. For testing I have used Xorg's modesetting driver and xf86-video-armsoc-omap5.

### Modesetting driver

Xorg's modesetting driver supports DRI3, but unfortunately only when using Glamor, which uses OpenGL for 2D acceleration. There's no OpenGL HW on TI's devices.

However, for testing purposes, it is possible to use Mesa SW rendering to fulfill the OpenGL requirement. With this this setup, SGX is used to render to a buffer, passed to the modesetting driver via DRI3, which is then composited to the screen using OpenGL SW rendering. Needless to say, the composition is very slow.

Although slow with SW rendering, modesetting driver is quite stable and the DRI3 support works fine, so it is good for testing.

### xf86-video-armsoc-omap5

Julien Boulnois has been working on xf86-video-armsoc-omap5, which provides 2D acceleration using the GC320 IP, and offers DRI3. The combination of SGX for 3D and GC320 for 2D should provide the best possible performance. However, xf86-video-armsoc-omap5 is still a work in progress, and much more unstable than the modesetting driver.

You can find xf86-video-armsoc-omap5 from:

https://github.com/julbouln/xf86-video-armsoc-omap5

### powervr.ini

The SGX driver tries to guess which WSEGL plugin library to use. However, I don't think this works very well for DRI3WSEGL. It's best to define the plugin explicitly in the powervr.ini file (either under /etc or under your home directory):

```
[default]
WindowSystem=libpvrDRI3WSEGL.so
```

### Note about Mesa

Mesa provides OpenGL ES, EGL and GBM libraries. These conflict with the libraries for SGX. It is possible to have both Mesa and SGX libraries installed, in different directories, but you need to be careful not to mix them. If an application uses one library from Mesa and one from SGX, you are sure to encounter interesting problems.

## dri3test

dri3test is a small hacky tool to study and test the DRI3 of an X server. It supports different ways to allocate the buffers, renders to those buffers using the CPU, and does page flipping of those buffers using DRI3. If you are not developing an X driver, you are probably not interested in this.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
