#!/bin/sh

../src/stop.sh
make -j48 librbd
make -j48
../src/vstart.sh -n
bin/ceph osd pool create rbd 75
bin/rbd pool init
bin/rbd import --export-format 2 StartTesting
sudo bin/rbd device map -t nbd StartTesting
sudo mount -osync /dev/nbd0 /mnt/testimage
