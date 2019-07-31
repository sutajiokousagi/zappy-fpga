#!/bin/bash

# convert the firmware bin into an uploadable blob
rm -f /tmp/ufirmware.upl
rm -f /tmp/ufirmware.bin

printf "\n\nPadding firmware image...\n"
# pad the firmware out to fill out the full firmware area
# the reason is that ufirmware.bin sometimes is not divisible by 4, which will
# cause the CRC computation to fail. So this forces a padding on ufirmware.bin
# which guarantees a deterministic fill for the entire firmware length and
# thus allow CRC to succeed
cp ../../firmware/firmware.bin /tmp/ufirmware.bin
dd if=/dev/zero of=/tmp/ufirmware.bin bs=1 count=1 seek=131071
./mkzappyimg -f --output /tmp/ufirmware.upl /tmp/ufirmware.bin

if [ $? -ne 0 ]
then
    printf "Could not pad firmware image, check permissions on /tmp. Press return to exit.\n"
    read dummy
    exit 1
fi

sudo openocd -c 'set BSCAN_FILE bscan_spi_xc7s50.bit' -c 'set FIRMWARE_FILE /tmp/ufirmware.upl' -f cl-firmware.cfg
sudo ./reboot.sh
