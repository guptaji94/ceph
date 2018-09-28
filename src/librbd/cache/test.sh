#!/bin/sh

# Attempt to stop any running ceph instances
../src/stop.sh
# Remove potentially write protected files in output
sudo rm -rf out/2018*

# Build
make -j48 librbd
make -j48

# Restart ceph
../src/vstart.sh -n
bin/ceph osd pool create rbd 75
bin/rbd pool init

# Import existing image
bin/rbd import --export-format 2 StartTesting

# Map rbd image and grab device name
nbd_device = $(sudo bin/rbd device map -t nbd StartTesting | grep -e "nbd[0-9]")

# Mount device and run tests
sudo mount -osync "/dev/$nbd_device" /mnt/testimage
cd /mnt/testimage/
dd if=/dev/zero of=zeros.txt obs=1024 count=240
./testDa
cd -

# Unmount
sudo umount /dev/$nbd_device
