#!/bin/bash
echo 68 > /sys/class/gpio/export
echo out > /sys/class/gpio/PC4/direction
echo 1 > /sys/class/gpio/PC4/value
echo 7 > /sys/devices/platform/backlight/backlight/backlight/brightness
modprobe g_ether
ifconfig usb0 up
ifconfig usb0 192.168.100.10

ln -sf /usr/share/zoneinfo/Asia/Taipei /etc/localtime

MAIN_PROGRAM="koli"
BACKUP_DIR="/root/backup"
PROGRAM_DIR="/root/koli"
PROGRAM_PATH="$PROGRAM_DIR/$MAIN_PROGRAM"

USB_DEVICE="/dev/sda1"
MOUNT_POINT="/mnt/usb"
if [ ! -d "$BACKUP_DIR" ]; then
    mkdir -p "$BACKUP_DIR"
    echo "creat backup path $BACKUP_DIR"
fi

if [ ! -d "$MOUNT_POINT" ]; then
    mkdir -p "$MOUNT_POINT"
    echo "creat mount path $MOUNT_POINT"
fi
if [ ! -d "/mnt/sd" ]; then
    mkdir -p "/mnt/sd"
    echo "create /mnt/sd path"
fi

if [ -e "$USB_DEVICE" ]; then

    mount $USB_DEVICE $MOUNT_POINT

    if [ -e "$MOUNT_POINT/$MAIN_PROGRAM" ]; then

        mv "$PROGRAM_PATH" "$BACKUP_DIR"
        cp "$MOUNT_POINT/$MAIN_PROGRAM" "$PROGRAM_PATH"

        echo "main backup and update sucess"
        umount $MOUNT_POINT
        sync
        echo "USB umount"
        cd "$PROGRAM_DIR" || exit 1

        if ! ./"$MAIN_PROGRAM"; then
            echo "update error..."

            mv "$BACKUP_DIR/$MAIN_PROGRAM" "$PROGRAM_PATH"

            cd "$PROGRAM_DIR" || exit 1
            if ! ./"$MAIN_PROGRAM"; then
                echo "backup main error"
            else
                echo "backup main sucess"
            fi
        fi
    else
        echo "mount USB error"
        cd /root/koli
        ./koli&
    fi
    
else
    echo "USB not found"
    cd /root/koli
    ./koli&
fi