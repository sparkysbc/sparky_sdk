#!/bin/sh
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       MIT
# The contents of this file are subject to the MIT license as set out below.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# Auto-generated for omap_linux from build: sgxddk_MAIN@3625561
#

# PVR Consumer services version number
#
PVRVERSION="sgxddk_MAIN@3625561"

# Where we record what we did so we can undo it.
#
DDK_INSTALL_LOG=/etc/powervr_ddk_install.log

# basic installation function
# $1=blurb
#
bail()
{
    echo "$1" >&2
    echo "" >&2
    echo "Installation failed" >&2
    exit 1
}

# basic installation function
# $1=fromfile, $2=destfilename, $3=blurb, $4=chmod-flags, $5=chown-flags
#
install_file()
{
	if [ ! -e $1 ]; then
	 	[ -n "$VERBOSE" ] && echo "skipping file $1 -> $2"
		 return
	fi
	
	DESTFILE=${DISCIMAGE}$2
	DESTDIR=`dirname $DESTFILE`

	$DOIT mkdir -p ${DESTDIR} || bail "Couldn't mkdir -p ${DESTDIR}"
	[ -n "$VERBOSE" ] && echo "Created directory `dirname $2`"

	# Delete the original so that permissions don't persist.
	$DOIT rm -f $DESTFILE
	$DOIT cp -f $1 $DESTFILE || bail "Couldn't copy $1 to $DESTFILE"
	$DOIT chmod $4 ${DISCIMAGE}$2
	$DOIT chown $5 ${DISCIMAGE}$2

	echo "$3 `basename $1` -> $2"
	$DOIT echo "file $2" >>${DISCIMAGE}${DDK_INSTALL_LOG}
}

# Install a symbolic link
# $1=fromfile, $2=destfilename
#
install_link()
{
	DESTFILE=${DISCIMAGE}$2
	DESTDIR=`dirname $DESTFILE`

	if [ ! -e ${DESTDIR}/$1 ]; then
		 [ -n "$VERBOSE" ] && echo $DOIT "skipping link ${DESTDIR}/$1"
		 return
	fi

	$DOIT mkdir -p ${DESTDIR} || bail "Couldn't mkdir -p ${DESTDIR}"
	[ -n "$VERBOSE" ] && echo "Created directory `dirname $2`"

	# Delete the original so that permissions don't persist.
	#
	$DOIT rm -f $DESTFILE

	$DOIT ln -s $1 $DESTFILE || bail "Couldn't link $1 to $DESTFILE"
	$DOIT echo "link $2" >>${DISCIMAGE}${DDK_INSTALL_LOG}
	[ -n "$VERBOSE" ] && echo " linked `basename $1` -> $2"
}

# Tree-based installation function
# $1 = fromdir $2=destdir $3=blurb
#
install_tree()
{
	if [ ! -z $INSTALL_TARGET ]; then
		# Use rsync and SSH to do the copy as it is way faster.
		echo "rsyncing $3 to root@$INSTALL_TARGET:$2"
		$DOIT rsync -crlpt -e ssh $1/* root@$INSTALL_TARGET:$2 || bail "Couldn't rsync $1 to root@$INSTALL_TARGET:$2"
	else 
		$DOIT mkdir -p ${DISCIMAGE}$2 || bail "Couldn't mkdir -p ${DISCIMAGE}$2"
		if [ -z "$DOIT" ]; then
			tar -C $1 -cf - . | tar -C ${DISCIMAGE}$2 -x${VERBOSE}f -
		else
			$DOIT "tar -C $1 -cf - . | tar -C ${DISCIMAGE}$2 -x${VERBOSE}f -"
		fi
	fi

	if [ $? = 0 ]; then
		echo "Installed $3 in ${DISCIMAGE}$2"
		$DOIT echo "tree $2" >>${DISCIMAGE}${DDK_INSTALL_LOG}
	else
		echo "Failed copying $3 from $1 to ${DISCIMAGE}$2"
	fi
}

# Uninstall something.
#
uninstall()
{
	if [ ! -f ${DISCIMAGE}${DDK_INSTALL_LOG} ]; then
		echo "Nothing to un-install."
		return;
	fi

	BAD=0
	VERSION=""
	while read type data; do
		case $type in
		version)	# do nothing
			echo "Uninstalling existing version $data"
			VERSION="$data"
			;;
		link|file) 
			if [ -z "$VERSION" ]; then
				BAD=1;
				echo "No version record at head of ${DISCIMAGE}${DDK_INSTALL_LOG}"
			elif ! $DOIT rm -f ${DISCIMAGE}${data}; then
				BAD=1;
			else
				[ -n "$VERBOSE" ] && echo "Deleted $type $data"
			fi
			;;
		tree)
		  if [ "${data}" = "/usr/local/XSGX" ]; then
		    $DOIT rm -Rf ${DISCIMAGE}${data}
		  fi
			;;
		esac
	done < ${DISCIMAGE}${DDK_INSTALL_LOG};

	if [ $BAD = 0 ]; then
		echo "Uninstallation completed."
		$DOIT rm -f ${DISCIMAGE}${DDK_INSTALL_LOG}
	else
		echo "Uninstallation failed!!!"
	fi
}

# Help on how to invoke
#
usage()
{
	echo "usage: $0 [options...]"
	echo ""
	echo "Options: -v            verbose mode"
	echo "         -n            dry-run mode"
	echo "         -u            uninstall-only mode"
	echo "         --no-pvr      don't install PowerVR driver components"
	echo "         --no-x        don't install X window system"
	echo "         --no-drm      don't install DRM libraries"
	echo "         --no-display  don't install integrated PowerVR display module"
	echo "         --no-bcdevice don't install buffer class device module"
	echo "         --root path   use path as the root of the install file system"
	exit 1
}

install_pvr()
{
	$DOIT echo "version sgxddk_MAIN@3625561" >${DISCIMAGE}${DDK_INSTALL_LOG}

	# Install the standard libraries
	#
	install_file libGLES_CM.so /usr/lib/libGLES_CM.so.1.14.3625561 "shared library" 0644 0:0
	install_link libGLES_CM.so.1.14.3625561 /usr/lib/libGLES_CM.so.1
	install_link libGLES_CM.so.1.14.3625561 /usr/lib/libGLES_CM.so


	install_file libusc.so /usr/lib/libusc.so.1.14.3625561 "shared library" 0644 0:0
	install_link libusc.so.1.14.3625561 /usr/lib/libusc.so.1
	install_link libusc.so.1.14.3625561 /usr/lib/libusc.so

	install_file libGLESv2.so /usr/lib/libGLESv2.so.1.14.3625561 "shared library" 0644 0:0
	install_link libGLESv2.so.1.14.3625561 /usr/lib/libGLESv2.so.1
	install_link libGLESv2.so.1.14.3625561 /usr/lib/libGLESv2.so
	install_link libGLESv2.so.1.14.3625561 /usr/lib/libGLESv2.so.2

	install_file libglslcompiler.so /usr/lib/libglslcompiler.so.1.14.3625561 "shared library" 0644 0:0
	install_link libglslcompiler.so.1.14.3625561 /usr/lib/libglslcompiler.so.1
	install_link libglslcompiler.so.1.14.3625561 /usr/lib/libglslcompiler.so

	install_file libPVROCL.so /usr/lib/libPVROCL.so.1.14.3625561 "shared library" 0644 0:0
	install_link libPVROCL.so.1.14.3625561 /usr/lib/libPVROCL.so.1
	install_link libPVROCL.so.1.14.3625561 /usr/lib/libPVROCL.so
	install_file liboclcompiler.so /usr/lib/liboclcompiler.so.1.14.3625561 "shared library" 0644 0:0
	install_link liboclcompiler.so.1.14.3625561 /usr/lib/liboclcompiler.so.1
	install_link liboclcompiler.so.1.14.3625561 /usr/lib/liboclcompiler.so



	install_file libIMGegl.so /usr/lib/libIMGegl.so.1.14.3625561 "shared library" 0644 0:0
	install_link libIMGegl.so.1.14.3625561 /usr/lib/libIMGegl.so.1
	install_link libIMGegl.so.1.14.3625561 /usr/lib/libIMGegl.so
	install_file libEGL.so /usr/lib/libEGL.so.1.14.3625561 "shared library" 0644 0:0
	install_link libEGL.so.1.14.3625561 /usr/lib/libEGL.so.1
	install_link libEGL.so.1.14.3625561 /usr/lib/libEGL.so
	install_file libpvr2d.so /usr/lib/libpvr2d.so.1.14.3625561 "shared library" 0644 0:0
	install_link libpvr2d.so.1.14.3625561 /usr/lib/libpvr2d.so.1
	install_link libpvr2d.so.1.14.3625561 /usr/lib/libpvr2d.so

	install_file libpvrPVR2D_BLITWSEGL.so /usr/lib/libpvrPVR2D_BLITWSEGL.so.1.14.3625561 "shared library" 0644 0:0
	install_link libpvrPVR2D_BLITWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_BLITWSEGL.so.1
	install_link libpvrPVR2D_BLITWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_BLITWSEGL.so
	install_file libpvrPVR2D_FLIPWSEGL.so /usr/lib/libpvrPVR2D_FLIPWSEGL.so.1.14.3625561 "shared library" 0644 0:0
	install_link libpvrPVR2D_FLIPWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_FLIPWSEGL.so.1
	install_link libpvrPVR2D_FLIPWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_FLIPWSEGL.so
	install_file libpvrPVR2D_FRONTWSEGL.so /usr/lib/libpvrPVR2D_FRONTWSEGL.so.1.14.3625561 "shared library" 0644 0:0
	install_link libpvrPVR2D_FRONTWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_FRONTWSEGL.so.1
	install_link libpvrPVR2D_FRONTWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_FRONTWSEGL.so
	install_file libpvrPVR2D_LINUXFBWSEGL.so /usr/lib/libpvrPVR2D_LINUXFBWSEGL.so.1.14.3625561 "shared library" 0644 0:0
	install_link libpvrPVR2D_LINUXFBWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_LINUXFBWSEGL.so.1
	install_link libpvrPVR2D_LINUXFBWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_LINUXFBWSEGL.so

	install_file libpvrPVR2D_DRIWSEGL.so /usr/lib/libpvrPVR2D_DRIWSEGL.so.1.14.3625561 "shared library" 0644 0:0
	install_link libpvrPVR2D_DRIWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_DRIWSEGL.so.1
	install_link libpvrPVR2D_DRIWSEGL.so.1.14.3625561 /usr/lib/libpvrPVR2D_DRIWSEGL.so

	install_file libdbm.so /usr/lib/libdbm.so.1.14.3625561 "shared library" 0644 0:0
	install_link libdbm.so.1.14.3625561 /usr/lib/libdbm.so.1
	install_link libdbm.so.1.14.3625561 /usr/lib/libdbm.so

	install_file libsrv_um.so /usr/lib/libsrv_um.so.1.14.3625561 "shared library" 0644 0:0
	install_link libsrv_um.so.1.14.3625561 /usr/lib/libsrv_um.so.1
	install_link libsrv_um.so.1.14.3625561 /usr/lib/libsrv_um.so
	install_file libsrv_init.so /usr/lib/libsrv_init.so.1.14.3625561 "shared library" 0644 0:0
	install_link libsrv_init.so.1.14.3625561 /usr/lib/libsrv_init.so.1
	install_link libsrv_init.so.1.14.3625561 /usr/lib/libsrv_init.so




	install_file libPVROGL_MESA.so /usr/lib/libPVROGL_MESA.so.1.14.3625561 "shared library" 0644 0:0
	install_link libPVROGL_MESA.so.1.14.3625561 /usr/lib/libPVROGL_MESA.so.1
	install_link libPVROGL_MESA.so.1.14.3625561 /usr/lib/libPVROGL_MESA.so
	install_file libegl4ogl.so /usr/lib/libegl4ogl.so.1.14.3625561 "shared library" 0644 0:0
	install_link libegl4ogl.so.1.14.3625561 /usr/lib/libegl4ogl.so.1
	install_link libegl4ogl.so.1.14.3625561 /usr/lib/libegl4ogl.so




	# Install the standard executables
	#

	install_file pvrsrvctl /usr/local/bin/pvrsrvctl "binary" 0755 0:0
	install_file sgx_init_test /usr/local/bin/sgx_init_test "binary" 0755 0:0



	# Install the standard unittests
	#


	install_file services_test /usr/local/bin/services_test "binary" 0755 0:0
	install_file sgx_blit_test /usr/local/bin/sgx_blit_test "binary" 0755 0:0
	install_file sgx_clipblit_test /usr/local/bin/sgx_clipblit_test "binary" 0755 0:0
	install_file sgx_flip_test /usr/local/bin/sgx_flip_test "binary" 0755 0:0
	install_file sgx_render_flip_test /usr/local/bin/sgx_render_flip_test "binary" 0755 0:0
	install_file pvr2d_test /usr/local/bin/pvr2d_test "binary" 0755 0:0



	install_file gles1test1 /usr/local/bin/gles1test1 "binary" 0755 0:0
	install_file gles1_texture_stream /usr/local/bin/gles1_texture_stream "binary" 0755 0:0

	install_file gles2test1 /usr/local/bin/gles2test1 "binary" 0755 0:0
	install_file glsltest1_vertshader.txt /usr/local/bin/glsltest1_vertshader.txt "shader" 0644 0:0
	install_file glsltest1_fragshaderA.txt /usr/local/bin/glsltest1_fragshaderA.txt "shader" 0644 0:0
	install_file glsltest1_fragshaderB.txt /usr/local/bin/glsltest1_fragshaderB.txt "shader" 0644 0:0
	install_file gles2_texture_stream /usr/local/bin/gles2_texture_stream "binary" 0755 0:0
	install_file eglinfo /usr/local/bin/eglinfo "binary" 0755 0:0

	install_file ocl_unit_test /usr/local/bin/ocl_unit_test "binary" 0755 0:0
	install_file ocl_filter_test /usr/local/bin/ocl_filter_test "binary" 0755 0:0



	install_file xtest /usr/local/bin/xtest "binary" 0755 0:0
	install_file xeglinfo /usr/local/bin/xeglinfo "binary" 0755 0:0

	install_file xgles1test1 /usr/local/bin/xgles1test1 "binary" 0755 0:0
	install_file xmultiegltest /usr/local/bin/xmultiegltest "binary" 0755 0:0
	install_file xgles1_texture_stream /usr/local/bin/xgles1_texture_stream "binary" 0755 0:0

	install_file xgles2test1 /usr/local/bin/xgles2test1 "binary" 0755 0:0
	install_file xgles2_texture_stream /usr/local/bin/xgles2_texture_stream "binary" 0755 0:0

	install_file xgltest1 /usr/local/bin/xgltest1 "binary" 0755 0:0
	install_file xgltest2 /usr/local/bin/xgltest2 "binary" 0755 0:0
	install_file gltest2_vertshader.txt /usr/local/bin/gltest2_vertshader.txt "shader" 0644 0:0
	install_file gltest2_fragshaderA.txt /usr/local/bin/gltest2_fragshaderA.txt "shader" 0644 0:0
	install_file gltest2_fragshaderB.txt /usr/local/bin/gltest2_fragshaderB.txt "shader" 0644 0:0


}

install_X()
{


	[ -d usr ] &&
		install_tree usr/local/XSGX /usr/local/XSGX "X.Org X server and libraries"
	[ -f pvr_drv.so ] &&
		install_file pvr_drv.so /usr/local/XSGX/lib/xorg/modules/drivers/pvr_drv.so "X.Org PVR DDX video module" 0755 0:0
	[ -f pvrinp_drv.so ] &&
		install_file pvrinp_drv.so /usr/local/XSGX/lib/xorg/modules/input/pvrinp_drv.so "X.Org PVR DDX input module" 0755 0:0
 	[ -f xorg.conf ] &&
		install_file xorg.conf /usr/local/XSGX/etc/xorg.conf "X.Org configuration file" 0644 0:0
  	[ -f pvr_dri.so ] &&
		install_file pvr_dri.so /usr/local/XSGX/lib/dri/pvr_dri.so "X.Org PVR DRI module" 0755 0:0
 }

# Work out if there are any special instructions.
#
while [ "$1" ]; do
	case "$1" in
	-v|--verbose)
		VERBOSE=v;
		;;
	-r|--root)
		DISCIMAGE=$2;
		shift;
		;;
	-t|--install-target)
		INSTALL_TARGET=$2;
		shift;
		;;
	-u|--uninstall)
		UNINSTALL=y
		;;
	-n)	DOIT=echo
		;;
	--no-pvr)
		NO_PVR=y
		;;
	--no-x)
		NO_X=y
		;;
	--no-drm)
		NO_DRM=y
		;;
	--no-display)
		NO_DISPLAYMOD=y
		;;
	--no-bcdevice)
		NO_BCDEVICE=y
		;;
	-h | --help | *)	
		usage
		;;
	esac
	shift
done

# Find out where we are?  On the target?  On the host?
#
case `uname -m` in
arm*)	host=0;
		from=target;
		DISCIMAGE=/;
		;;
sh*)	host=0;
		from=target;
		DISCIMAGE=/;
		;;
i?86*)	host=1;
		from=host;
		if [ -z "$DISCIMAGE" ]; then	
			echo "DISCIMAGE must be set for installation to be possible." >&2
			exit 1
		fi
		;;
x86_64*)	host=1;
		from=host;
		if [ -z "$DISCIMAGE" ]; then	
			echo "DISCIMAGE must be set for installation to be possible." >&2
			exit 1
		fi
		;;
*)		echo "Don't know host to perform on machine type `uname -m`" >&2;
		exit 1
		;;
esac

if [ ! -z "$INSTALL_TARGET" ]; then
	if ssh -q -o "BatchMode=yes" root@$INSTALL_TARGET "test 1"; then
		echo "Using rsync/ssh to install to $INSTALL_TARGET"
	else
		echo "Can't access $INSTALL_TARGET via ssh."
		# We have to use the `whoami` trick as this script is often run with 
		# sudo -E
		if [ ! -e ~`whoami`/.ssh/id_rsa.pub ] ; then
			echo " You need to generate a public key for root via ssh-keygen"
			echo " then append it to root@$INSTALL_TARGET:~/.ssh/authorized_keys."
		else
			echo "Have you installed root's public key into root@$INSTALL_TARGET:~/.ssh/authorized_keys?"
			echo "You can do so by executing the following as root:"
			echo "ssh root@$INSTALL_TARGET \"mkdir -p .ssh; cat >> .ssh/authorized_keys\" < ~/.ssh/id_rsa.pub"
		fi
		echo "Falling back to copy method."
		unset INSTALL_TARGET
	fi
fi

if [ ! -d "$DISCIMAGE" ]; then
	echo "$0: $DISCIMAGE does not exist." >&2
	exit 1
fi

echo
echo "Installing PowerVR Consumer/Embedded DDK 'sgxddk_MAIN@3625561' on $from"
echo
echo "File system installation root is $DISCIMAGE"
echo

# Uninstall whatever's there already.
#
uninstall
[ -n "$UNINSTALL" ] && exit

#  Now start installing things we want.
#
[ -z "$NO_PVR" ] && install_pvr
[ -z "$NO_X" ] && install_X

# All done...
#
echo 
echo "Installation complete!"
if [ "$host" = 0 ]; then
   echo "You may now reboot your target."
fi
echo
