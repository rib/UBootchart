#!/bin/sh

VERSION=0.8

echo "ubootchart: finished...."
echo "ubootchart: Copylog data to /var/log/ubootchart"

if ! test -d /var/log/ubootchart; then
    mkdir -p /var/log/ubootchart
fi

# Log some basic information about the system.
(
    echo "version = $VERSION"
    echo "title = Boot chart for $( hostname | sed q ) ($( date ))"
    echo "system.uname = $( uname -srvm | sed q )"
    if [ -f /etc/gentoo-release ]; then
            echo "system.release = $( sed q /etc/gentoo-release )"
    elif [ -f /etc/SuSE-release ]; then
            echo "system.release = $( sed q /etc/SuSE-release )"
    elif [ -f /etc/debian_version ]; then
            echo "system.release = Debian GNU/$( uname -s ) $( cat /etc/debian_version )"
    elif [ -f /etc/frugalware-release ]; then
            echo "system.release = $( sed q /etc/frugalware-release )"
    elif [ -f /etc/pardus-release ]; then
            echo "system.release = $( sed q /etc/pardus-release )"
    else
            echo "system.release = $( sed 's/\\.//g;q' /etc/issue )"
    fi

    # Get CPU count
    local cpucount=$(grep -c '^processor' /proc/cpuinfo)
    if [ $cpucount -gt 1 -a -n "$(grep 'sibling.*2' /proc/cpuinfo)" ]; then
            # Hyper-Threading enabled
            cpucount=$(( $cpucount / 2 ))
    fi
    if grep -q '^model name' /proc/cpuinfo; then
            echo "system.cpu = $( grep '^model name' /proc/cpuinfo | sed q )"\
                 "($cpucount)"
    else
            echo "system.cpu = $( grep '^cpu' /proc/cpuinfo | sed q )"\
                 "($cpucount)"
    fi

    echo "system.kernel.options = $( sed q /proc/cmdline )"
) >> header


#rm -f /var/log/ubootchart/*
cp * /var/log/ubootchart
cd /var/log/ubootchart

echo "ubootchart: creating /var/log/ubootchart/bootchart.tgz"
tar -zcf bootchart.tgz header kernel_pacct *.log


