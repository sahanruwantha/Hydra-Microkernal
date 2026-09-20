#!/usr/bin/env bash
#
# boot_to_shell.sh - cold-boot FSD-Amoeba in QEMU and drive it all the way
# to the `amoeba%` prompt, swapping the three install floppies through the
# QEMU monitor. Leaves QEMU running in the background with its monitor on
# $SOCK so later steps can keep driving it with amdrive.sh.
#
# The install floppies must be swapped in order:
#   KERNELS-INSTALL -> (boot kernel) -> RAMDISK-0 -> RAMDISK-1 -> shell
#
# Usage: ./boot_to_shell.sh [screenshot-name]
#
set -u
D=/home/sahan/amoeba-ref
SOCK=/tmp/amoeba-mon.sock
SHOT=${1:-booted}

source "$D/amdrive.sh"

# Tear down any previous instance.
pkill -f 'qemu-system-i386.*amoeba-hd.img' 2>/dev/null
sleep 2
rm -f "$SOCK"

nohup qemu-system-i386 \
    -fda "$D/extract/floppies/KERNELS-INSTALL.image" -boot a -m 64 \
    -hda "$D/amoeba-hd.img" -cdrom "$D/fsd-2002B.iso" \
    -display none -monitor unix:"$SOCK",server,nowait \
    -net none > "$D/qemu.log" 2>&1 &

sleep 6
amkey ret                 # boot the default COLD kernel
sleep 32                  # kernel init, drivers, "insert RAMDISK-0"

mon "change floppy0 $D/extract/floppies/RAMDISK-0.image raw"
sleep 2; amkey ret
sleep 45                  # first half of the compressed ramdisk

mon "change floppy0 $D/extract/floppies/RAMDISK-1.image raw"
sleep 2; amkey ret
sleep 80                  # second half, then servers start

amshot "$SHOT"
echo "booted; screenshot: $D/$SHOT.png"
