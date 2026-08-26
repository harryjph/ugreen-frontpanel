#!/bin/bash
# Drive iDX6011 Pro front-panel HDD LEDs from plain Linux.
# Reimplements the disk-LED behaviour from the factory firmware.
#
# LED semantics reproduced (reverse-engineered):
#   no disk        -> off
#   disk present   -> white, solid          (color 0 = #FFFFFF)
#   disk I/O       -> blink 200ms/800ms     (trigger timer2)
#   locate request -> blue flash            (write N to ./locate to start, for 30 s)
# Run as root:  ./hdd-leds.sh

set -u
LEDS=/sys/class/leds
POLL=1          # seconds between presence checks
BLINK_ON=250    # ms; factory-firmware behaviour: ~200/800ms
BLINK_OFF=750
LOCATE_SECS=30
STATE=/run/hdd-leds.state       # cache: which LEDs are currently "present-solid"

get_state() { [ -f "$STATE" ] && cat "$STATE"; }
set_state() { : > "$STATE"; printf '%s\n' "$@" > "$STATE"; }

led() { # led <name> <attr>=<value>...
    local d="$LEDS/$1"; shift
    for kv in "$@"; do
        echo "${kv#*=}" > "$d/${kv%%=*}"
    done
}

disk_present() {   # <sdX|nvmeNnM> -> 0 if present & not swap/system-only? adjust filters
    [ -e "/sys/block/$1" ]
}

scan_disks() {
    ls /sys/block 2>/dev/null | grep -E '^(sd[a-z]+|nvme[0-9]+n[0-9]+)$' | sort
}

locate_start() {  # <diskN> — blink blue like the UGOS "locate" button
    local d="disk$1"
    led "$d" trigger=timer2 color=4 brightness=255 \
        delay_on=$BLINK_ON delay_off=$BLINK_OFF
}
locate_stop() {
    local d="disk$1"
    led "$d" trigger=normal color=0 brightness=255
}

apply_one() {     # <disk name idx> <present 0|1>
    local n="$1" p="$2" d="disk$n"
    if [ "$p" = 1 ]; then
        led "$d" trigger=normal color=0 brightness=255
    else
        led "$d" trigger=none brightness=0
    fi
}

echo "hdd-leds: watching ${LEDS}/disk*"
while sleep "$POLL"; do
    disks=$(scan_disks)
    newstate=""
    i=1
    changed=0
    while read -r dev; do
        [ -z "$dev" ] && continue
        newstate="$newstate $i"
        if ! get_state | grep -qx "$i"; then apply_one "$i" 1; changed=1; fi
        i=$((i+1))
        [ $i -gt 6 ] && break               # 6 bays on iDX6011 Pro
    done <<< "$disks"
    while [ $i -le 6 ]; do                  # empty slots must go dark
        if get_state | grep -qx "$i" || [ "$changed" = 1 ]; then apply_one "$i" 0; fi
        i=$((i+1))
    done
    set_state $newstate
done
