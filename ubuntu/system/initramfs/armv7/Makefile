IMG_CPIO=../initrd_v7.cpio
all:
	find . ! -path "./.git*" \
	       ! -path "./README.md" \
	       ! -path "./Makefile" \
	       ! -path "./Makefile" \
	       ! -path "./sbin/thin*" \
	       ! -path "./sbin/lvm" \
	       ! -path "./sbin/dmsetup" \
	       ! -path "./lib/libdevmapper*" \
	| cpio -H newc  -ov > ${IMG_CPIO}

