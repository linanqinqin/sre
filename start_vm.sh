#!/bin/bash


sudo qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    -smp 2 \
    -m 4G \
    -device virtio-scsi-pci,id=scsi0 \
    -device scsi-hd,drive=hd0,id=disk0 \
    -drive file=/opt/u20s.qcow2,if=none,aio=native,cache=none,format=qcow2,id=hd0 \
    -net user,hostfwd=tcp::8080-:22 \
    -net nic,model=virtio \
    -daemonize \
    -qmp unix:./qmp.sock,server,nowait \
    -vnc localhost:0 \
    -audiodev none,id=snd0 
