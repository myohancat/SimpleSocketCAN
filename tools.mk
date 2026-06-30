DISTRO          := telechips
ARCH            := aarch64-$(DISTRO)-linux

# NOT YOCTO BUILD
ifeq ($(TOPDIR),)
PN              ?= tc-micom-manager

TOPDIR          := $(YOCTO_DIR)/build
ROOTDIR         := $(BUILDDIR)/tmp/work/aarch64-$(DISTRO)-linux/$(PN)/1.0-r0
CROSS_COMPILE   := $(ROOTDIR)/recipe-sysroot-native/usr/bin/$(ARCH)/$(ARCH)-

SYSROOT_DIR     := $(ROOTDIR)/recipe-sysroot
PKG_CONFIG_PATH := "$(SYSROOT_DIR)/usr/lib/pkgconfig:$(SYSROOT_DIR)/usr/lib/$(ARCH)/pkgconfig:$(SYSROOT_DIR)/usr/share/pkgconfig"
PKG_CONFIG_SYSROOT_DIR := $(SYSROOT_DIR)

AR              := $(CROSS_COMPILE)ar
AS              := $(CROSS_COMPILE)as
LD              := $(CROSS_COMPILE)ld
NM              := $(CROSS_COMPILE)nm
CC              := $(CROSS_COMPILE)gcc -mcpu=cortex-a72+crc+crypto -fstack-protector-strong -fPIE -pie -Wformat -Wformat-security -Werror=format-security -fPIC --sysroot=$(SYSROOT_DIR)
GCC             := $(CROSS_COMPILE)gcc -mcpu=cortex-a72+crc+crypto -fstack-protector-strong -fPIE -pie -Wformat -Wformat-security -Werror=format-security -fPIC --sysroot=$(SYSROOT_DIR)
CPP             := $(CROSS_COMPILE)cpp -mcpu=cortex-a72+crc+crypto -fstack-protector-strong -fPIE -pie -Wformat -Wformat-security -Werror=format-security -fPIC --sysroot=$(SYSROOT_DIR)
CXX             := $(CROSS_COMPILE)g++ -mcpu=cortex-a72+crc+crypto -fstack-protector-strong -fPIE -pie -Wformat -Wformat-security -Werror=format-security -fPIC --sysroot=$(SYSROOT_DIR)
FC              := $(CROSS_COMPILE)gfortran
RANLIB          := $(CROSS_COMPILE)ranlib
READELF         := $(CROSS_COMPILE)readelf
STRIP           := $(CROSS_COMPILE)strip
OBJCOPY         := $(CROSS_COMPILE)objcopy
OBJDUMP         := $(CROSS_COMPILE)objdump
INSTALL         := install

PKG_CONFIG      := PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) PKG_CONFIG_SYSROOT_DIR=$(PKG_CONFIG_SYSROOT_DIR) pkg-config

CFLAGS          := -g -Wall -Wextra -Werror -Wuninitialized
CXXFLAGS        := -g -Wall -Wextra -Werror -Wuninitialized
LDFLAGS         :=

MAKEFLAGS       +="-j -l $(shell grep -c ^processor /proc/cpuinfo) "
endif
