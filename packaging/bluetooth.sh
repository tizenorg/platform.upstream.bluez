#!/bin/sh
BLUETOOTH_CONFIG=/etc/sysconfig/bluetooth
test -r $BLUETOOTH_CONFIG && . $BLUETOOTH_CONFIG

if [ "$START_BLUETOOTHD" = "no" ]; then
	exit 0
fi

exec /usr/sbin/bluetoothd --udev
