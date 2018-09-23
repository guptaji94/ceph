#!/bin/sh

../src/stop.sh
#make -j48 librbd
#make -j48
../src/vstart.sh -n
bin/ceph osd pool create rbd 75
bin/rbd pool init
bin/rbd create --size 1024 testimage
sudo bin/rbd device map -t nbd testimage
sudo mkfs.ext4 -b 1024 /dev/nbd0
sudo mount -osync /dev/nbd0 /mnt/testimage
