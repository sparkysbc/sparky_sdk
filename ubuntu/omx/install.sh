#!/bin/sh
#@author lishiyuan
# install openmax release file
#

# kodi  version number
#
OMXVERSION="omx-1.0.0"

# Where we record what we did so we can undo it.
#
OMX_INSTALL_LOG=/etc/omx_install.log

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
	$DOIT echo "file $2" >>${DISCIMAGE}${OMX_INSTALL_LOG}
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
	$DOIT echo "link $2" >>${DISCIMAGE}${OMX_INSTALL_LOG}
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
		$DOIT echo "tree $2" >>${DISCIMAGE}${OMX_INSTALL_LOG}
	else
		echo "Failed copying $3 from $1 to ${DISCIMAGE}$2"
	fi
}

# Uninstall something.
#
uninstall()
{
	if [ ! -f ${DISCIMAGE}${OMX_INSTALL_LOG} ]; then
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
				echo "No version record at head of ${DISCIMAGE}${OMX_INSTALL_LOG}"
			elif ! $DOIT rm -f ${DISCIMAGE}${data}; then
				BAD=1;
			else
				[ -n "$VERBOSE" ] && echo "Deleted $type $data"
			fi
			;;
		tree)
		  if [ "${data}" = "/usr/local/lib/kodi" ]; then
		    $DOIT rm -Rf ${DISCIMAGE}${data}
		  fi
			;;
		esac
	done < ${DISCIMAGE}${OMX_INSTALL_LOG};

	if [ $BAD = 0 ]; then
		echo "Uninstallation completed."
		$DOIT rm -f ${DISCIMAGE}${OMX_INSTALL_LOG}
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
	echo "         --root path   use path as the root of the install file system"
	exit 1
}


install_omx()
{
	$DOIT echo "version $OMXVERSION" >${DISCIMAGE}${OMX_INSTALL_LOG}
	install_file omx_codec.xml /etc/omx_codec.xml "openmax configuration file" 0755 0:0

	#install video codec library
	install_file vd_vc1.so /usr/lib/vd_vc1.so "shared library" 0644 0:0
	install_file vd_xvid.so /usr/lib/vd_xvid.so "shared library" 0644 0:0
	install_file vd_vp8.so /usr/lib/vd_vp8.so "shared library" 0644 0:0
	install_file vd_vp6.so /usr/lib/vd_vp6.so "shared library" 0644 0:0
	install_file vd_rv34.so /usr/lib/vd_rv34.so "shared library" 0644 0:0
	install_file vd_msm4.so /usr/lib/vd_msm4.so "shared library" 0644 0:0
	install_file vd_mjpg.so /usr/lib/vd_mjpg.so "shared library" 0644 0:0
	install_file vd_h264.so /usr/lib/vd_h264.so "shared library" 0644 0:0
	install_file vd_h263.so /usr/lib/vd_h263.so "shared library" 0644 0:0
	install_file vd_flv1.so /usr/lib/vd_flv1.so "shared library" 0644 0:0
	install_file vd_mpeg.so /usr/lib/vd_mpeg.so "shared library" 0644 0:0
	install_file libACT_EncAPI.so /usr/lib/libACT_EncAPI.so "shared library" 0644 0:0
	
	install_file libOMX.Action.Video.Decoder.so /usr/lib/libOMX.Action.Video.Decoder.so "shared library" 0644 0:0
	install_file libOMX.Action.Video.Encoder.so /usr/lib/libOMX.Action.Video.Encoder.so "shared library" 0644 0:0
	install_file libOMX_Core.so /usr/lib/libOMX_Core.so "shared library" 0644 0:0
	install_file libion.so /usr/lib/libion.so "shared library" 0644 0:0
	install_file libvde_core.so /usr/lib/libvde_core.so "shared library" 0644 0:0
	install_file libalc.so /usr/lib/libalc.so "shared library" 0644 0:0

	install_tree omx-include /usr/include/omx-include "omx header files"

	#install examples code and depend libraries, header files
	install_file examples/lib/libavcodec.so.56.26.100 /usr/lib/libavcodec.so.56.26.100 "shared library" 0644 0:0
	install_link libavcodec.so.56.26.100 /usr/lib/libavcodec.so.56
	install_link libavcodec.so.56.26.100 /usr/lib/libavcodec.so
	install_file examples/lib/libavdevice.so.56.4.100 /usr/lib/libavdevice.so.56.4.100 "shared library" 0644 0:0
	install_link libavdevice.so.56.4.100 /usr/lib/libavdevice.so.56
	install_link libavdevice.so.56.4.100 /usr/lib/libavdevice.so
	install_file examples/lib/libavfilter.so.5.11.102 /usr/lib/libavfilter.so.5.11.102 "shared library" 0644 0:0
	install_link libavfilter.so.5.11.102 /usr/lib/libavfilter.so.5
	install_link libavfilter.so.5.11.102 /usr/lib/libavfilter.so
	install_file examples/lib/libavformat.so.56.25.101 /usr/lib/libavformat.so.56.25.101 "shared library" 0644 0:0
	install_link libavformat.so.56.25.101 /usr/lib/libavformat.so.56
	install_link libavformat.so.56.25.101 /usr/lib/libavformat.so
	install_file examples/lib/libavutil.so.54.20.100 /usr/lib/libavutil.so.54.20.100 "shared library" 0644 0:0
	install_link libavutil.so.54.20.100 /usr/lib/libavutil.so.54
	install_link libavutil.so.54.20.100 /usr/lib/libavutil.so
	install_file examples/lib/libswresample.so.1.1.100 /usr/lib/libswresample.so.1.1.100 "shared library" 0644 0:0
	install_link libswresample.so.1.1.100 /usr/lib/libswresample.so.1
	install_link libswresample.so.1.1.100 /usr/lib/libswresample.so

	install_tree examples/ffmpeg/libavcodec /usr/include/libavcodec "libavcodec header files"
	install_tree examples/ffmpeg/libavdevice /usr/include/libavdevice "libavdevice header files"
	install_tree examples/ffmpeg/libavfilter /usr/include/libavfilter "libavfilter header files"
	install_tree examples/ffmpeg/libavformat /usr/include/libavformat "libavformat header files"
	install_tree examples/ffmpeg/libavutil /usr/include/libavutil "libavutil header files"
	install_tree examples/owlplayer /home/owlplayer "owlplayer source code"
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
echo "Installing $OMXVERSION on $from"
echo
echo "File system installation root is $DISCIMAGE"
echo

# Uninstall whatever's there already.
#
uninstall
[ -n "$UNINSTALL" ] && exit

#  Now start installing things we want.
#
install_omx

# All done...
#
echo 
echo "Installation complete!"

echo
