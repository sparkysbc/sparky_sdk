

R_OS_DIR=$(TOP_DIR)/../ubuntu
MODULES_DIR=$(R_OS_DIR)/modules

rootfs:
	$(Q)mkdir -p $(IMAGE_DIR)/
	$(Q)rm -rf $(MODULES_DIR)/
	$(Q)mkdir -p $(MODULES_DIR)/
	$(Q)make -C $(KERNEL_SRC) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) O=$(KERNEL_OUT_DIR) INSTALL_MOD_PATH=$(MODULES_DIR) modules_install
	$(Q)$(MAKE) -C $(R_OS_DIR) BOARD_NAME=$(BOARD_NAME) rootfs
	$(Q)mv $(R_OS_DIR)/system.img $(IMAGE_DIR)/

initramfs:
	$(Q)$(MAKE) -C $(R_OS_DIR) BOARD_NAME=$(BOARD_NAME) initramfs
	$(Q)mkdir -p $(MISC_DIR)/
	$(Q)mv $(R_OS_DIR)/ramdisk.img $(MISC_DIR)/
