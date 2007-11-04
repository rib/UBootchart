#!/bin/sh

echo "ubootchart: finished...."
echo "ubootchart: Copylog data to $log_dir"


if ! test -d $log_dir; then
    mkdir -p $log_dir
    if test $? -ne 0; then
        echo "Failed to create $log_dir"
        exit 1
    fi
fi

# Log some basic information about the system.
(
    echo "version = $version"
    echo "title = Boot chart for $( hostname | sed q ) ($( date ))"
    echo "system.uname = $( uname -srvm | sed q )"
    if test -f /etc/gentoo-release; then
            echo "system.release = $( sed q /etc/gentoo-release )"
    elif test -f /etc/SuSE-release; then
            echo "system.release = $( sed q /etc/SuSE-release )"
    elif test -f /etc/debian_version; then
            echo "system.release = Debian GNU/$( uname -s ) $( cat /etc/debian_version )"
    elif test -f /etc/frugalware-release; then
            echo "system.release = $( sed q /etc/frugalware-release )"
    elif test -f /etc/pardus-release; then
            echo "system.release = $( sed q /etc/pardus-release )"
    else
            echo "system.release = $( sed 's/\\.//g;q' /etc/issue )"
    fi

    # Get CPU count
    cpucount=$(grep -c '^processor' /proc/cpuinfo)
    if test $cpucount -gt 1 -a -n "$(grep 'sibling.*2' /proc/cpuinfo)"; then
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


kernel_pacct=""
if test -f kernel_pacct; then
	kernel_pacct="kernel_pacct"
fi

cp header $kernel_pacct *.log $log_dir
cd $log_dir

echo "ubootchart: creating $log_dir/bootchart.tgz"
tar -zcf bootchart.tgz header $kernel_pacct *.log


