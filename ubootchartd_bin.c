/* 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Copyright (C) 2007 Robert Bragg <my first name @sixbynine.org>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/mount.h>
#include <dirent.h>
#include <string.h>
#include <time.h>

/* If DEBUG enabled then print to stdout as well as
 * error.log */
#if defined(DEBUG)
#define PRINTF(ARGS...) printf(ARGS)
#else
#define PRINTF(ARGS...)
#endif

static FILE *error_log;
#define UB_PRINT(MSG...) \
    do{ \
	PRINTF("ubootchart: " MSG); \
	if(error_log) { \
	    fprintf(error_log, "ubootchart: " MSG); \
	} \
    }while(0)
#define UB_PERROR(MSG) \
    do{ \
	PRINTF("ubootchart: " MSG ": %s func=%s,line=%u\n", \
		error_str, __FUNCTION__, __LINE__ ); \
	if(error_log) { \
	const char *error_str = strerror(errno); \
	fprintf(error_log, "ubootchart: " MSG ": %s func=%s,line=%u\n", \
		error_str, __FUNCTION__, __LINE__ ); \
	} \
    }while(0)

#define TRUE 1
#define FALSE 0

typedef struct _Buffer
{
    char *buf;
    unsigned int size;
    unsigned int space;
}Buffer;

typedef struct _UBootChartState
{
    struct _config{
	char *init_prog;
	char *start_script;
	char *finish_script;
	char *log_dir;
	char *tmpfs_lazy_mnt_dir;
	unsigned int probe_interval;
	unsigned int proc_stat_log_buf_size;
	unsigned int proc_diskstats_log_buf_size;
	unsigned int proc_ps_log_buf_size;
    }config;

    struct _proc{
	int stat_fd;
	int diskstats_fd;
	int uptime_fd;
    }proc;

    struct _buffers{
	Buffer proc_stat;
	Buffer proc_diskstats;
	Buffer proc_ps;
    }buffers;
}UBootChartState;


int sigusr1_recieved;


static char *
read_config_string(const char *key, const char *default_value)
{
    char *ret;

    ret = getenv(key);
    if(ret)
    {
	ret = strdup(ret);
    }
    else
    {
	if(default_value)
	    ret = strdup(default_value);
    }
    return ret;
}


static unsigned int
read_config_uint(const char *key, unsigned int default_value)
{
    char *tmp_str;
    unsigned int ret;
    
    tmp_str = getenv(key);
    if(tmp_str)
    {
	ret = (unsigned int)strtoul(tmp_str, NULL, 10);
    }
    else
    {
	ret = default_value;
    }
    return ret;
}


static void
read_config(UBootChartState *state)
{
    state->config.init_prog =
	read_config_string("init_prog", "/sbin/init");
    state->config.start_script = read_config_string("start_script", NULL);
    state->config.finish_script = read_config_string("finish_script", NULL);
    state->config.log_dir = read_config_string("log_dir",
					     "/var/log/ubootchart");
    state->config.tmpfs_lazy_mnt_dir = read_config_string("tmpfs_lazy_mnt_dir",
							  NULL);

    state->config.probe_interval = read_config_uint("probe_interval", 200);
    state->config.proc_stat_log_buf_size =
	read_config_uint("proc_stat_log_buf_size", 51200);
    state->config.proc_diskstats_log_buf_size =
	read_config_uint("proc_diskstats_log_buf_size", 409600);
    state->config.proc_ps_log_buf_size =
	read_config_uint("proc_ps_log_buf_size", 1638400);
}


static void
run_start_script(UBootChartState *state)
{
    int status;
    
    if(!state->config.start_script)
	return;
	
    status = system(state->config.start_script);
    if(status == -1)
    {
	UB_PERROR("Failed to run start script");
    }
}


/* We mount a tmpfs, chdir into it and immediatly lazy unmount it.
 * (That means no one else sees the mount)
 * This way hopfully we dont get in the way of any init scripts */
#ifndef MNT_DETACH
#define MNT_DETACH 0x02
#endif
static void
enter_tmpfs_dir(UBootChartState *state)
{
    int status;
    
    if(!state->config.tmpfs_lazy_mnt_dir)
	return;

    status = mount("none",
		   state->config.tmpfs_lazy_mnt_dir,
		   "tmpfs",
		   MS_NOATIME|MS_NODIRATIME,
		   NULL);
    if(status == -1)
    {
	UB_PERROR("Failed to mount tmpfs");
    }
    status = chdir(state->config.tmpfs_lazy_mnt_dir);
    if(status == -1)
    {
	UB_PERROR("Failed to enter tmpfs");
    }

    /* FIXME - make this a config option (handy for debugging
     * since we don't want to loose the error log if ubootchartd_bin
     * crashes) - note you wouldn't use /mnt if not lazy unmounting
     */
    status = umount2(state->config.tmpfs_lazy_mnt_dir, MNT_FORCE|MNT_DETACH);
    if(status == -1)
    {
	UB_PERROR("Failed to lazy unmount tmpfs");
    }
}


static void
open_error_log(UBootChartState *state)
{
    error_log = fopen("./errors.log", "a");
    if(!error_log)
    {
	perror("Failed to open errors.log");
    }    
}


static void
handle_sigusr1(int signal)
{
    sigusr1_recieved=TRUE;
}


static void
register_sigusr1_handler(void)
{
    sigusr1_recieved=FALSE;
    signal(SIGUSR1, handle_sigusr1);
}


static const char *
get_current_uptime(int uptime_fd, int *uptime_len)
{
    static char buf[100];
    char *bufp = buf;
    int len;
    int ret;
    unsigned long tv_sec, tv_nsec;
    int count;

    /* FIXME - can we just use gettimeofday here?
     * - Need to look at the bootchart scripts to see how
     *   they interpret this. 
     * - At least we should only need to read this once
     *   at startup and from that point we can use gettimeofday
     */

    len = sizeof(buf);
    while(len!=0 && (ret = read(uptime_fd, bufp, len)) != 0)
    {
	if(ret == -1)
	{
	    if(errno == EINTR)
	    {
		continue;
	    }
	    UB_PERROR("Failed to read uptime entry");
	    return "0";
	}
	len -= ret;
	bufp += ret;
    }

    if(ret != 0)
    {
	UB_PRINT("read more data than expected from /proc/uptime");
    }

    ret = lseek(uptime_fd, 0, SEEK_SET);
    if(ret == -1)
    {
	UB_PERROR("Failed to seek back to the start of /proc/uptime");
    }

    count = sscanf(buf, "%lu.%02lu ", &tv_sec, &tv_nsec);
    if(count != 2)
    {
	UB_PRINT("Failed to read /proc/uptime properly");
    }

    *uptime_len =
	snprintf(buf,
		 sizeof(buf),
		 "\n%lu%02lu\n",
		 tv_sec,
		 tv_nsec);

    return buf;
}

static int
extend_buffer(Buffer *buffer, const char *log_name)
{
    buffer->buf = realloc(buffer->buf, buffer->size*2);
    if(!buffer->buf)
    {
	UB_PRINT("Failed to realloc memory for log buffer\n");
	return FALSE;
    }
    buffer->space += buffer->size;
    buffer->size *= 2;

    UB_PRINT("Warning we had to resize the buffer for %s to %u bytes. "
	     "Suggest you update your ubootchart.conf\n",
	     log_name, buffer->size);
    return TRUE;
}


static void
read_proc_to_buffer(int proc_fd,
		    Buffer *buffer,
		    char *log_name)
{
    char *buf = buffer->buf;
    unsigned int size = buffer->size;
    unsigned int space = buffer->space;
    int ret=0;

    buf += (size - space);
    
    while((ret = read(proc_fd, buf, space)) != 0)
    {
	if(ret == -1)
	{
	    if(errno == EINTR)
	    {
		continue;
	    }
	    UB_PERROR("Failed to read proc entry");
	    UB_PRINT("> file=%s, size=%u, space=%u\n",
		     log_name, size, space);
	    return;
	}
	space -= ret;
	buf += ret;

	if(space == 0)
	{
	    buffer->space = space;
	    if(!extend_buffer(buffer, log_name))
	    {
		break;
	    }
	    buf = buffer->buf;
	    space = buffer->space;
	    size = buffer->size;
	    buf += (size - space);
	}
    }
    buffer->space = space;
}


static void
log_process_stat_files(UBootChartState *state)
{
    DIR *proc_dir;
    struct dirent *current_dir;

    proc_dir = opendir("/proc");

    while((current_dir = readdir(proc_dir)))
    {
	int current_stat_fd;
	char current_stat_file[100];
	int ret;

	if(current_dir->d_name[0] >= '0' &&
	   current_dir->d_name[1] <= '9')
	{
	    int count;
	    count = snprintf(current_stat_file,
			     sizeof(current_stat_file),
			     "/proc/%s/stat",
			     current_dir->d_name);
	    if(count == 100)
	    {
		/* Shouldn't ever happen, but it's certainly not a pid */
		continue;
	    }

	    /* open the stat file */
	    current_stat_fd =
		open(current_stat_file, O_RDONLY);
	    if(current_stat_fd == -1)
	    {
		/* We ignore these errors, since boot processes
		 * come and go quickly and what we get back from
		 * readdir is quite often gone by this point */
		continue;
	    }

	    read_proc_to_buffer(current_stat_fd,
				&state->buffers.proc_ps,
				current_stat_file);

	    /* close the stat file */
	    ret = close(current_stat_fd);
	    if(ret == -1)
	    {
		UB_PERROR("Failed to close /proc/PID/stat file");
	    }
	}
    }

    closedir(proc_dir);
}


static void
append_uptime_to_log_buffer(Buffer *buffer,
			    const char *uptime,
			    int uptime_len,
			    const char *log_name)
{
    char *buf;
    
    if(buffer->space < uptime_len)
    {
	if(!extend_buffer(buffer, log_name))
	    return;
    }
    buf = buffer->buf;
    buf += (buffer->size - buffer->space);
    memcpy(buf, uptime, uptime_len);
    buffer->space -= uptime_len;
}


static int
do_system_probe(UBootChartState *state)
{
    int offset;
    const char *uptime;
    int uptime_len=0;

    //UB_PRINT("System Probe!\n");

    uptime = get_current_uptime(state->proc.uptime_fd, &uptime_len);


    append_uptime_to_log_buffer(&state->buffers.proc_stat,
				uptime, uptime_len,
				"proc_stat.log");
    read_proc_to_buffer(state->proc.stat_fd,
			&state->buffers.proc_stat,
			"proc_stat.log");
    offset=lseek(state->proc.stat_fd, 0, SEEK_SET);
    if(offset == -1)
    {
	UB_PERROR("Failed to seek back to the start of proc_stat.log");
    }


    append_uptime_to_log_buffer(&state->buffers.proc_diskstats,
				uptime, uptime_len,
				"proc_diskstats.log");
    read_proc_to_buffer(state->proc.diskstats_fd,
			&state->buffers.proc_diskstats,
			"proc_diskstats.log");
    offset=lseek(state->proc.diskstats_fd, 0, SEEK_SET);
    if(offset == -1)
    {
	UB_PERROR("Failed to seek back to the start of proc_diskstats.log");
    }

    append_uptime_to_log_buffer(&state->buffers.proc_ps,
				uptime, uptime_len,
				"proc_ps.log");
    log_process_stat_files(state);

    return TRUE;
}


static void
alloc_log_buffers(UBootChartState *state)
{
    state->buffers.proc_stat.size = state->config.proc_stat_log_buf_size;
    state->buffers.proc_stat.buf = malloc(state->buffers.proc_stat.size);
    if(!state->buffers.proc_stat.buf)
    {
	UB_PRINT("Not enough mem for proc_stat.log accumulation buffer\n");
    }
    state->buffers.proc_stat.space=state->buffers.proc_stat.size;
    
    state->buffers.proc_diskstats.size = state->config.proc_diskstats_log_buf_size;
    state->buffers.proc_diskstats.buf = malloc(state->buffers.proc_diskstats.size);
    if(!state->buffers.proc_diskstats.buf)
    {
	UB_PRINT("Not enough mem for proc_diskstats.log accumulation buffer\n");
    }
    state->buffers.proc_diskstats.space=state->buffers.proc_diskstats.size;

    state->buffers.proc_ps.size = state->config.proc_ps_log_buf_size;
    state->buffers.proc_ps.buf = malloc(state->buffers.proc_ps.size);
    if(!state->buffers.proc_ps.buf)
    {
	UB_PRINT("Not enough mem for proc_ps.log accumulation buffer\n");
    }
    state->buffers.proc_ps.space=state->buffers.proc_ps.size;
}


static int
open_proc_file(const char *proc_file)
{
    int ret;
    ret = open(proc_file, O_RDONLY);
    if(ret == -1)
    {
	UB_PERROR("Failed to open proc entry");
	UB_PRINT("> proc entry = %s\n", proc_file);
    }
    return ret;
}


static void
open_proc_files(UBootChartState *state)
{
    state->proc.stat_fd = open_proc_file("/proc/stat");
    state->proc.diskstats_fd = open_proc_file("/proc/diskstats");
    state->proc.uptime_fd = open_proc_file("/proc/uptime");
}


static void
start_process_accounting(void)
{
    int ret;

    ret = creat("./kernel_pacct", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if(ret == -1)
    {
	UB_PERROR("Failed to create ./kernel_pacct");
	return;
    }
    close(ret);

    ret = acct("kernel_pacct");
    if(ret == -1)
    {
	UB_PERROR("Failed to switch on process accounting");
    }
}


static void
stop_process_accounting(void)
{
    int ret;
    ret = acct(NULL);
    if(ret == -1)
    {
	UB_PERROR("Failed to switch off process accounting");
    }

}


static void
run_finish_script(UBootChartState *state)
{
    int status;

    if(!state->config.finish_script)
	return;

    status = system(state->config.finish_script);
    if(status == -1)
    {
	UB_PERROR("Failed to run finish script");
    }
}


static void
close_file(int fd)
{
    int status;
    status = close(fd);
    if(status == -1)
    {
	UB_PERROR("Failed to close file");
    }  
}


static void
write_buffer_to_log(int log_fd, Buffer *buffer)
{
    int ret;
    int len = buffer->size - buffer->space;
    ret = write(log_fd, buffer->buf, len);
    if(ret == -1)
    {
	UB_PERROR("Failed to write log");
    }
    else if(ret != len)
    {
	UB_PRINT("Failed to write log (no errno)");
    }    
}


static void
write_log_files(UBootChartState *state)
{
    int proc_stat_log_fd;
    int proc_diskstats_log_fd;
    int proc_ps_log_fd;

    proc_stat_log_fd =
	open("proc_stat.log",
	     O_WRONLY|O_APPEND|O_CREAT|O_TRUNC,
	     S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if(proc_stat_log_fd == -1)
    {
	UB_PERROR("Failed to open proc_stat.log");
    }
    write_buffer_to_log(proc_stat_log_fd, &state->buffers.proc_stat);

    proc_diskstats_log_fd =
	open("proc_diskstats.log",
	     O_WRONLY|O_APPEND|O_CREAT|O_TRUNC,
	     S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if(proc_diskstats_log_fd == -1)
    {
	UB_PERROR("Failed to open proc_diskstats.log");
    }
    write_buffer_to_log(proc_diskstats_log_fd, &state->buffers.proc_diskstats);

    proc_ps_log_fd =
	open("proc_ps.log",
	     O_WRONLY|O_APPEND|O_CREAT|O_TRUNC,
	     S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if(proc_ps_log_fd == -1)
    {
	UB_PERROR("Failed to open proc_ps.log");
    }
    write_buffer_to_log(proc_ps_log_fd, &state->buffers.proc_ps);
    
    close_file(state->proc.stat_fd);
    close_file(state->proc.diskstats_fd);

    close_file(proc_stat_log_fd);
    close_file(proc_diskstats_log_fd);
    close_file(proc_ps_log_fd);
}


int
main(int argc, char **argv)
{
    UBootChartState state;
    struct timespec delay;
    
    read_config(&state);
    enter_tmpfs_dir(&state);
    open_error_log(&state);
    start_process_accounting();
    run_start_script(&state);
    register_sigusr1_handler();
    alloc_log_buffers(&state);
    open_proc_files(&state);
    
    delay.tv_sec=state.config.probe_interval/1000;
    delay.tv_nsec=(state.config.probe_interval%1000)*1000000;
    while(1)
    {
	if(sigusr1_recieved)
	    break;
	
	nanosleep(&delay, NULL);
	do_system_probe(&state);
    }
    
    stop_process_accounting();
    
    write_log_files(&state);
    run_finish_script(&state);

    return 0;
}


