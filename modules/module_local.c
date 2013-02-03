/*****************************************************************************
 * MusicDB - Copyright Eric Lorimer (8/02)
 *
 * This file implements a MusicDB module to serve local files from a database
 *
 * see the file README for some explanation of how it works.
 *
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * **************************************************************************/
#include "config.h"

#include <stdio.h>
#include <sys/stat.h>
#include <utime.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <includes.h>
#include <vfs.h>
#include "mysql.h"
#include "shortcut.h"
#include "musicdb.h"

struct file {
	int usage;
	int systemfd;
};

static struct file fd_table[1024];
static char prefix[] = "/home/music/";


/****************************************************************************
 * int module_init()
 *
 * clear out the file descriptor table
 * *************************************************************************/
int module_init()
{
	int i;

	for (i=0; i < 1024; i++)
		fd_table[i].usage = 0;

	return 1;
}


/****************************************************************************
 * int module_open()
 *
 * initialize a file descriptor and return a pointer from the given entry
 * *************************************************************************/
int module_open(struct song_entry *data)
{
	int fd;
	char realpath[256];

	for (fd = 1; fd < 1024 && fd_table[fd].usage; fd++)
			;
	DEBUG(1, ("module_open: fname = %s, nfields = %d\n", data->fname, data->nfields));
	snprintf(realpath, 256, "%s%s", prefix, data->fields[0]);
	fd_table[fd].systemfd = open(realpath, O_RDONLY);
	DEBUG(1, ("module_open: open(%s) returned %d\n", realpath, fd_table[fd].systemfd));
	if ( fd_table[fd].systemfd == 0 )
		return 0;
	fd_table[fd].usage = 1;
	return fd;
}


/****************************************************************************
 * int module_close()
 *
 * simple; pass on to system
 * *************************************************************************/
int module_close(int fd)
{
	if ( fd > 0 && fd < 1024 ) {
		close(fd_table[fd].systemfd);
		fd_table[fd].usage = 0;
	}
	return 0;
}


/****************************************************************************
 * int module_read()
 *
 * simple; pass on to local filesystem
 * *************************************************************************/
ssize_t module_read(int fd, void *data, size_t n)
{
	if ( fd < 0 || fd > 1024 || fd_table[fd].usage < 1 ) {
		errno = EBADF;
		return -1;
	}
	return read(fd_table[fd].systemfd, data, n);
}


/****************************************************************************
 * int module_lseek()
 *
 * lseek the file
 * *************************************************************************/
SMB_OFF_T module_lseek(int fd, SMB_OFF_T offset, int whence)
{
	if ( fd < 0 || fd > 1024 || fd_table[fd].usage < 1 ) {
		errno = EBADF;
		return -1;
	}
	return lseek(fd_table[fd].systemfd, offset, whence);
}


/****************************************************************************
 * int module_fstat()
 *
 * return stat information from an open file descriptor
 * *************************************************************************/
int module_fstat(int fd, SMB_STRUCT_STAT *sbuf)
{
	if ( fd < 0 || fd > 1024 || fd_table[fd].usage < 1 ) {
		errno = EBADF;
		return -1;
	}

	return fstat(fd_table[fd].systemfd, sbuf);
}
