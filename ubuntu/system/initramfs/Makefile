IMG_CPIO=initrd_v7.cpio
IMG_SRC=initrd_v7.cpio.gz
IMG_DST=ramdisk.img

all:
	make -C armv7 all
	@dd if=${IMG_CPIO} bs=512 | gzip -v9 > ${IMG_SRC}
	@mkimage -n "RAMFS" -A arm -O linux -T ramdisk -C gzip -a 02000000 -e 02000000 -d ${IMG_SRC} ${IMG_DST}
	@rm ${IMG_CPIO} ${IMG_SRC}
