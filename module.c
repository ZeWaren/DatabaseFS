/*****************************************************************************
 * MusicDB - Copyright Eric Lorimer (8/02)
 *
 * This file implements a MusicDB module to create Windows shortcuts
 * dynamically and serve them to clients.
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
	char *uname;
	char *title, *artist;
	int hits;
	char *data;
	int filesize;
	SMB_OFF_T offset;
};

static struct file fd_table[1024];
static struct shortcut lnk_template;


/****************************************************************************
 * int module_init()
 *
 * basically just loads the shortcut template used to build shortcuts
 * *************************************************************************/
int module_init()
{
	int i, rc;
	char *template;

	for (i=0; i < 1024; i++)
		fd_table[i].usage = 0;

	/* load the shortcut template from disk */
	template = get_string_option("template");
	rc = load_shortcut_template(template, &lnk_template);
	if ( rc != 0 )
		DEBUG(1, ("musicdb_module_init load_shortcut_template failed; shortcuts will be broken\n"));
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

	for (fd = 1; fd < 1024 && fd_table[fd].usage; fd++)
			;
	fd_table[fd].data = (char *)MALLOC(1024);
	fd_table[fd].filesize = easy_shortcut(data->fields[0], data->fields[1],
			data->fields[2], &lnk_template, fd_table[fd].data, 1024);
	fd_table[fd].offset = 0;
	fd_table[fd].usage = 1;
	return fd;
}


/****************************************************************************
 * int module_close()
 *
 * close a file freeing the file descriptor and associated memory
 * *************************************************************************/
int module_close(int fd)
{
	if ( fd > 0 && fd < 1024 ) {
		FREE(fd_table[fd].data);
		fd_table[fd].usage = 0;
	}
	return 0;
}


/****************************************************************************
 * int module_read()
 *
 * read some data from the file given by fd
 * *************************************************************************/
ssize_t module_read(int fd, void *data, size_t n)
{
	struct file *pfile;

	if ( fd < 0 || fd > 1024 || fd_table[fd].usage < 1 ) {
		errno = EBADF;
		return -1;
	}
	pfile = &fd_table[fd];

	if ( pfile->offset + n > pfile->filesize ) {
		/* short read */
		memcpy(data, &pfile->data[pfile->offset],
				pfile->filesize - pfile->offset);
		return (pfile->filesize - pfile->offset);
	} else {
		memcpy(data, &pfile->data[pfile->offset], n);
		return (ssize_t)n;
	}
	return 0;
}


/****************************************************************************
 * int module_lseek()
 *
 * lseek the file
 * *************************************************************************/
SMB_OFF_T module_lseek(int fd, SMB_OFF_T offset, int whence)
{
	struct file *pfile;
	if ( fd < 0 || fd > 1024 || fd_table[fd].usage < 1 ) {
		errno = EBADF;
		return -1;
	}
	pfile = &fd_table[fd];

	switch (whence) {
		case SEEK_SET:
			pfile->offset = (offset > pfile->filesize) ? pfile->filesize : offset;
			break;
		case SEEK_CUR:
			pfile->offset = (pfile->offset + offset > pfile->filesize) ? pfile->filesize : pfile->offset + offset;
			break;
		case SEEK_END:
			pfile->offset = pfile->filesize;
			break;
	}
	return pfile->offset;
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

	memset(sbuf, 0, sizeof(SMB_STRUCT_STAT));
	sbuf->st_dev = 770;             // ??
	sbuf->st_ino = 1;               // Fake inode
	sbuf->st_mode = 0100644;
	sbuf->st_uid = 0;
	sbuf->st_gid = 0;
	sbuf->st_nlink = 1;             // number of hard links
	sbuf->st_size = fd_table[fd].filesize;
	sbuf->st_blksize = 4096;        // Physical block size
	sbuf->st_blocks = 1;            // Number of blocks allocated
	sbuf->st_atime = 1027382400;
	sbuf->st_mtime = 1027382400;
	sbuf->st_ctime = 1027382400;
	return 0;
}
