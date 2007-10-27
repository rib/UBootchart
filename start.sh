#!/bin/sh

echo "ubootchart: starting...."

if ! test -f /proc/cpuinfo; then
    echo "ubootchart: mounting /proc"
    mount -t proc none /proc
fi

