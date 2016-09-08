USB_NET_PATH=/sys/class/net/
RX_CPU_PATH=/queues/rx-0/rps_cpus
TX_CPU_PATH=/queues/tx-0/xps_cpus
USB_EP=/device/bNumEndpoints
XHCI_IRQ=/proc/irq/55/smp_affinity
CPU_MAP=7


set_usbnet_cpus() {
	local cpu_path
	local value
	cpu_path="${USB_NET_PATH}""$1""$USB_EP"
	if [ -f "$cpu_path" ]
	then
		cpu_path="${USB_NET_PATH}""$1""${RX_CPU_PATH}"
		value=$(cat  $cpu_path)
		#if [ $value != "7" ]
		#then
			echo 8 > $XHCI_IRQ
			echo $CPU_MAP > $cpu_path
		#fi

		cpu_path="${USB_NET_PATH}""$1""${TX_CPU_PATH}"
		value=$(cat  $cpu_path)
                #if [ $value != "7" ]
                #then
                        echo $CPU_MAP > $cpu_path
                #fi
	fi
}

check_usb_net(){
	filelist=$(ls $USB_NET_PATH )
	for dir in $filelist
	do
		if [ -d "$USB_NET_PATH""$dir" ];
		then
			set_usbnet_cpus $dir
		fi
	done
}

i=1
while [ $i ]
do
	check_usb_net
	sleep 3
done



