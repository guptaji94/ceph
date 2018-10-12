#!/bin/bash

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
bin/rbd import --export-format 2 StartMTesting #StartTesting

# Map rbd image and grab device name
#nbd_device=$(sudo bin/rbd device map -t nbd StartTesting | grep -e 'nbd[0-9]')
nbd_device=$(sudo bin/rbd device map -t nbd StartMTesting | grep -e 'nbd[0-9]')
echo "Mapped image to $nbd_device"

# Mount device and run tests
sudo mount -osync "$nbd_device" /mnt/testimage
cd /mnt/testimage/
logfile=$(ls -t out/client* | head -1)
echo "Active logfile: $logfile"
# tail -f logfile | grep -a "write_count" &
# tail -f logfile | grep -a "Total accesses" &
# sudo dd if=/dev/zero of=zeros.txt obs=1024 count=250
# sudo ./testDA
# cd -

# Unmount
# sudo umount $nbd_device

# wait
