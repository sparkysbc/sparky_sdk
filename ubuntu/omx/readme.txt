Here are sample steps and tips for install video decoder omx lib and examples code.
Tell developer how to compile examples code in S500 broad.
update 2015-07-09 by lishiyuan

step 1 Download omx_lib
	$ git clone ssh://192.168.4.4:29418/ZH/actions/GL5206/linux/omx_lib
	
step 2 Copy omx_lib to S500 broad
	 a)compress omx_lib to omx_lib.tar.gz
	 b)copy omx_lib.tar.gz to S500 broad directory /home 

step 3 Install omx_lib , header files and examples code
	$cd /home
	$tar xfz omx_lib.tar.gz
	In below directory /home/omx_lib execute 
	$./install.sh

	Remember: 
	a)omx header files be installed in /usr/include/omx-include
	b)omx video decoder libraries be installed in /usr/lib
	c)examples code be installed in /home/owlplayer
	
step 4 Compile examples
	cd /home/owlplayer
	$make

step 5 Test examples
	cd /home/owlplayer
	$./owlplayer "video file name directory"
	for example: ./owlplayer /home/test.mpeg

step 6 Test result
	when testing  ./owlplayer /home/test.mpeg , you will find a file "NV12_wxxx_hxxx" in /home.
	The file "NV12_wxxx_hxxx"  records 50 frames of the video "test.mpeg". 
	The "NV12" means the pix format is NV12. the "xxx" means width or height.
	You can play the file "NV12_wxxx_hxxx" by YUVPlayer.exe in Windows PC.