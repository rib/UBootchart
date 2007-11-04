#!/bin/sh

export version="HEAD"
unset tmpfs_lazy_mnt_dir
unset start_script
export finish_script=./finish.sh
export log_dir=./test_logs
export probe_interval=200
export proc_stat_log_buf_size=100
export proc_diskstats_log_buf_size=100
export proc_ps_log_buf_size=100

chmod +x ./finish.sh
rm errors.log

####################################################
# Now start capturing the stats:
####################################################
./ubootchartd_bin

