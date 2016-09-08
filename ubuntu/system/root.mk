
.PHONY: all rootfs initramfs clean

Q=

TOP_DIR=$(shell pwd)
ROOTFS_DIR=$(TOP_DIR)/rootfs_tmp
PATCH_DIR=$(TOP_DIR)/rootfs_patch
MODULES_DIR=$(TOP_DIR)/modules
OMX_DIR=$(TOP_DIR)/omx
KODI_DIR=$(TOP_DIR)/kodi
GPU_DIR=$(TOP_DIR)/gpu/v1
SYS_DIR=$(TOP_DIR)/system

ifeq ($(BOARD_NAME), )
    BOARD_NAME=gb5_wxga
endif

include $(SYS_DIR)/boards/$(BOARD_NAME)/config

rootfs:
	$(Q)rm -f $(TOP_DIR)/system.img
	$(Q)rm -rf $(ROOTFS_DIR)
	
	$(Q)tar jxvf $(SYS_DIR)/ubuntu1204.tar.bz2 -C $(TOP_DIR)/
	$(Q)mkdir -p $(ROOTFS_DIR)
	$(Q)sudo mount -o loop $(TOP_DIR)/system.img $(ROOTFS_DIR)
	
	$(Q)mkdir -p $(MODULES_DIR)
	$(Q)sudo rm -rf $(ROOTFS_DIR)/lib/modules/*
	$(Q)sudo cp -rf $(MODULES_DIR)/* $(ROOTFS_DIR)
	$(Q)sudo rm $(ROOTFS_DIR)/lib/modules/3.10.37/build
	$(Q)sudo rm $(ROOTFS_DIR)/lib/modules/3.10.37/source
	#$(Q)sudo rm -rf $(ROOTFS_DIR)/lib/modules/3.10.37/kernel/drivers/input/touchscreen
	
	$(Q)cd $(OMX_DIR) && sudo ./install.sh -r $(ROOTFS_DIR)
	
	$(Q)sudo mkdir $(ROOTFS_DIR)/pack
	$(Q)sudo cp -rf $(GPU_DIR)/* $(ROOTFS_DIR)/pack
	$(Q)cd $(ROOTFS_DIR)/pack && sudo chown -R root:root . && sudo ./install.sh -r $(ROOTFS_DIR)
	$(Q)sudo rm -rf $(ROOTFS_DIR)/pack
	
	$(Q)cd $(ROOTFS_DIR)/usr/bin && sudo ln -sf /usr/local/XSGX/bin/X X && sudo ln -sf /usr/local/XSGX/bin/Xorg Xorg
	$(Q)sudo cp $(ROOTFS_DIR)/usr/local/XSGX/etc/xorg.conf $(ROOTFS_DIR)/etc/
	$(Q)sudo rm $(ROOTFS_DIR)/usr/local/XSGX/lib/libXfixes.so*
	$(Q)cd $(ROOTFS_DIR)/usr/lib && sudo ln -sf /usr/local/XSGX/lib/libGL.so.1.2 libGL.so.1.2
	$(Q)cd $(ROOTFS_DIR)/usr/lib && sudo ln -sf /usr/local/XSGX/lib/libGL.so.1.2 libGL.so.1
	
	$(Q)mkdir $(PATCH_DIR)
	$(Q)cp -r $(SYS_DIR)/boards/common/rootfs/* $(PATCH_DIR)
	$(Q)if [ -d "$(SYS_DIR)/boards/$(BOARD_NAME)/rootfs" ]; then \
				cp -r $(SYS_DIR)/boards/$(BOARD_NAME)/rootfs/* $(PATCH_DIR) ; \
			fi
	$(Q)sudo chown -R root:root $(PATCH_DIR)
	$(Q)sudo cp -r $(PATCH_DIR)/* $(ROOTFS_DIR)
	$(Q)sudo rm -rf $(PATCH_DIR)
	
	$(Q)sudo umount $(ROOTFS_DIR)
	$(Q)sudo chmod 777 $(TOP_DIR)/system.img
	$(Q)rm -rf $(ROOTFS_DIR)

initramfs:
	$(Q)$(MAKE) -C $(SYS_DIR)/initramfs
	$(Q)mv $(SYS_DIR)/initramfs/ramdisk.img $(TOP_DIR)/

all: rootfs initramfs

