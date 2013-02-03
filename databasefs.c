/*****************************************************************************
 * DatabaseFS - Copyright Eric Lorimer (5/02)
 *
 * DatabaseFS is a samba VFS module connecting to a MySQL database and allowing
 * dynamic database browsing
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

//static struct vfs_ops default_vfs_ops;   /* For passthrough operation */
//static struct smb_vfs_handle_struct *musicdb_handle;
extern userdom_struct current_user_info;

MYSQL mysql;


/***************************************************************************
 * int musicdb_connect
 *
 * connect to the mysql database mp3 as user samba
 * *************************************************************************/
//static int musicdb_connect(struct connection_struct *conn, const char *service,
//	const char *user)    
static int musicdb_connect(struct vfs_handle_struct *musicdb_handle,
	struct connection_struct *conn, const char *service, const char *user)    
{
	char *username, *password;
	int entries, i = 0;
	struct musicdb_ctx *mydata;

	report_mem_usage("connect");
	/* start out by initializing our private data */
	mydata = (struct musicdb_ctx *)MALLOC(sizeof(struct musicdb_ctx));
	mydata->memory = talloc_init("musicdb");
	mydata->dtable = talloc(mydata->memory, sizeof(struct dirtbl));
	musicdb_handle->data = mydata;

	parse_config(lp_parm_const_string(SNUM(conn), musicdb_handle->param, "config_file", "databasefs.conf"));
	username = get_string_option("user");
	password = get_string_option("password");

	if ( !(mydata->sock = mysql_connect(&mysql, NULL, username, password)) ) {
		DEBUG(1, ("mysql_connect failed for user %s.\n", username));
		return -1;
	}

	if ( mysql_select_db(mydata->sock, get_string_option("database")) ) {
		DEBUG(1, ("mysql_select_db failed.\n"));
		return -1;
	}

	/* load dirtbl into memory */
	mydata->dtable->rows = (struct dirtbl_row *)
				talloc(mydata->memory, sizeof(struct dirtbl_row) * DIRTBL_ROWS);
	mydata->dtable->size = DIRTBL_ROWS;

	entries = load_dirtbl(mydata->sock, mydata->dtable);
	/* hmm... apparently the last mysql_select_db call does not execute fast
	 * enough. There must be a better way to check the status of the database
	 * before executing the query, but this works for now. big hack */
	while ( entries == 0 && i < 5 ) {
		entries = load_dirtbl(mydata->sock, mydata->dtable);
		i++;
	}

	/* check that this host name is allowed */
	/*
	if ( check_black_list(get_remote_machine_name(), mydata->sock) )
		smb_panic("User blacklisted.");
	*/

	/* now initialize the cache */
	mydata->cache = (ubi_cacheRoot *)
						talloc(mydata->memory, sizeof(ubi_cacheRoot));
	if ( mydata->cache == 0 ) {
		DEBUG(1, ("musicdb: mydata->cache initialization failed!\n"));
		return -1;
	}
	ubi_cacheInit(mydata->cache, compare_nodes, killnode, CACHE_ENTRIES,
			CACHE_ROWS);
	/* we set the entry_size as the number of rows */

	if ( !module_init() )
		smb_panic("FATAL error. unable to initialize module\n");

	return 0;
}


/**************************************************************************
 * void musicdb_disconnect
 *
 * closes the mysql database socket
 * ************************************************************************/
static void musicdb_disconnect(struct vfs_handle_struct *musicdb_handle, struct connection_struct *conn)
{
	struct musicdb_ctx *mydata = (struct musicdb_ctx *)musicdb_handle->data;

	ubi_cacheClear(mydata->cache);
	mysql_close(mydata->sock);
	talloc_destroy(mydata->memory);
	cleanup_config();
//	SAFE_FREE(mydata);
	FREE(mydata);
	report_mem_usage("disconnect");
	return;
}


/**************************************************************************
 * DIR *musicdb_opendir
 *
 * open the directory given by fname by executing a SQL statement
 * ************************************************************************/
static DIR *musicdb_opendir(struct vfs_handle_struct *musicdb_handle, struct connection_struct *conn, const char *fname)
{
	struct directory *dirptr =
			(struct directory *)MALLOC(sizeof(struct directory));
	struct dirtbl_row *drow;
	struct strstack stack;
	char newfname[128];
	struct cache_node *cache_entry;
	struct musicdb_ctx *mydata = (struct musicdb_ctx *)musicdb_handle->data;

	stack.stackptr = 0;
	/* if it doesn't end in a slash, tack one on */
	strncpy(newfname, fname, 128);
	if ( newfname[strlen(fname)-1] != '/' ) {
		newfname[strlen(fname)] = '/';
		newfname[strlen(fname)+1] = '\0';
	}
	if ( !search(1, newfname, &drow, &stack, mydata->dtable) )
		return NULL;

	if ( drow->subptr != -1 ) {
		dirptr->dtable_ptr = dirtbl_search(drow->subptr, mydata->dtable);
		dirptr->sdirid = drow->subptr;
	} else
		dirptr->dtable_ptr = -1;

	if ( drow->fileqry[0] == '\0' )
		dirptr->filequery = NULL;
	else {
		pstring filledqry;
		replace(drow->fileqry, &stack, filledqry, mydata->sock);
		cache_entry = (struct cache_node *)ubi_cacheGet(mydata->cache, filledqry);
		if ( cache_entry == NULL ) {
			/* couldn't find it in the cache. we have to load it */
			dirptr->filequery = (struct song_entry *)
					cache_load(SONG, filledqry, mydata->sock, mydata->cache);
		} else {
			/* found in the cache */
			dirptr->filequery = (struct song_entry *)
				ubi_slFirst((ubi_slListPtr)cache_entry->data);
		}
	}

	if ( drow->dirqry[0] == '\0' )
		dirptr->dirquery = NULL;
	else {
		pstring filledqry;
		replace(drow->dirqry, &stack, filledqry, mydata->sock);
		cache_entry = (struct cache_node *)ubi_cacheGet(mydata->cache, filledqry);
		if ( cache_entry == NULL ) {
			/* couldn't find it in the cache. we have to load it */
			dirptr->dirquery = (struct dir_entry *)
					cache_load(DDIR, filledqry, mydata->sock, mydata->cache);
		} else {
			/* found in the cache */
			dirptr->dirquery = (struct dir_entry *)
				ubi_slFirst((ubi_slListPtr)cache_entry->data);
		}
	}

	return (DIR *)dirptr;
}


/**************************************************************************
 * struct dirent *musicdb_readdir
 *
 * read and return the directory contents from the mysql result pointer
 * ************************************************************************/
static struct dirent *musicdb_readdir(struct vfs_handle_struct *musicdb_handle, struct connection_struct *conn, DIR *dirp)
{
	struct directory *dirptr = (struct directory *)dirp;
	struct musicdb_ctx *mydata = (struct musicdb_ctx *)musicdb_handle->data;
	static struct dirent dirbuf; 
	struct song_entry *filerow;
	int ptr;
	
	memset(&dirbuf, 0, sizeof(struct dirent));		// zero the struct
	if ( (filerow = dirptr->filequery) ) {
		strncpy(dirbuf.d_name, filerow->fname, 255);
		dirptr->filequery = (struct song_entry *)ubi_slNext(dirptr->filequery);
		return &dirbuf;
	}

	if ( dirptr->dirquery ) {
		snprintf(dirbuf.d_name, 255, "%s", dirptr->dirquery->template);
		dirptr->dirquery = (struct dir_entry *)ubi_slNext(dirptr->dirquery);
		return &dirbuf;
	}

	ptr = dirptr->dtable_ptr;
	if ( ptr > -1 && ptr < mydata->dtable->size &&
			mydata->dtable->rows[ptr].dirid == dirptr->sdirid) {
		snprintf(dirbuf.d_name, 255, "%s", mydata->dtable->rows[ptr].template);
		dirbuf.d_name[strlen(dirbuf.d_name) - 1] = '\0';
		/* chop the trailing / */
		dirptr->dtable_ptr++;
		return &dirbuf;
	}

	return NULL;			// no more data
}


/**************************************************************************
 * int musicdb_closedir
 *
 * free the mysql result handle for this directory.
 * ************************************************************************/
static int musicdb_closedir(struct vfs_handle_struct *musicdb_handle, struct connection_struct *conn, DIR *dirp)
{
//	SAFE_FREE(dirp);
	FREE(dirp);

	return 0;
}


/**************************************************************************
 * int musicdb_open
 *
 * search the directory for the file and then pass it on to the module
 * ************************************************************************/
static int musicdb_open(struct vfs_handle_struct *musicdb_handle, struct connection_struct *conn, const char *fname, int flags, mode_t mode)
{
	char base[512], leaf[128], *ptr;
	struct musicdb_ctx *mydata = (struct musicdb_ctx *)musicdb_handle->data;
	struct dirtbl_row *drow;
	struct strstack sstack;
	pstring filledqry;
	struct cache_node *cache_entry;
	struct song_entry *songs;

	sstack.stackptr = 0;
	strncpy(base, fname, 512);
	if ( (ptr = strrchr(base, '/')) != NULL ) {
		strncpy(leaf, ptr+1, 128);
		*(ptr+1) = '\0';
	} else {
		/* is this correct? can we get a stat request for a file with no /? */
		strncpy(leaf, base, 128);
		strncpy(base, "./", 512);
	}

	if ( !search(1, base, &drow, &sstack, mydata->dtable)  ) {
		errno = ENOENT;
		return -1;
	}
	if ( drow->fileqry[0] == '\0' ) {
		errno = EISDIR;
		return -1;
	}

	/* check if we have the query stored in the cache */
	/* we need to break this into a function as it is duplicated above in
	 * opendir */
	replace(drow->fileqry, &sstack, filledqry, mydata->sock);
	cache_entry = (struct cache_node *)ubi_cacheGet(mydata->cache, filledqry);
	if ( cache_entry == NULL ) {
		/* couldn't find it in the cache. we have to load it */
		songs = (struct song_entry *)
				cache_load(SONG, filledqry, mydata->sock, mydata->cache);
	} else {
		/* found in the cache */
		songs = (struct song_entry *)
			ubi_slFirst((ubi_slListPtr)cache_entry->data);
	}

	/* now traverse through songs and find the file */
	while ( songs ) {
		if ( strncmp(songs->fname, leaf, 128) == 0 )
			return module_open(songs);
		songs = (struct song_entry *)ubi_slNext(songs);
	}

	errno = ENOENT;
	return -1;
}


static int musicdb_close(struct vfs_handle_struct *musicdb_handle, struct files_struct *fsp, int fd)
{
	return module_close(fd);
}


static ssize_t musicdb_read(struct vfs_handle_struct *musicdb_handle, struct files_struct *fsp, int fd, void *data, size_t n)
{
	return module_read(fd, data, n);
}


static SMB_OFF_T musicdb_lseek(struct vfs_handle_struct *musicdb_handle, struct files_struct *fsp, int fd, SMB_OFF_T offset, int whence)
{
	return module_lseek(fd, offset, whence);
}


/**************************************************************************
 * int musicdb_stat
 *
 * Lookup the data for the filename. Need to add caching.
 * ************************************************************************/
static int musicdb_stat(struct vfs_handle_struct *musicdb_handle,
	struct connection_struct *conn, const char *fname, SMB_STRUCT_STAT *sbuf)
{
	struct dirtbl_row *drow;
	struct strstack sstack;
	char *p, leaf[128], base[512];
	struct musicdb_ctx *mydata = (struct musicdb_ctx *)musicdb_handle->data;
	pstring filequery, dirquery;
	
	if ( !strcmp(fname, ".") )
		return generic_dir_stat(sbuf);
	sstack.stackptr = 0;
	/* split fname into base and leaf */
	strncpy(base, fname, 512);
	if ( (p = strrchr(base, '/')) != NULL ) {
		strncpy(leaf, p+1, 128);
		*(p+1) = '\0';
	} else {
		/* is this correct? can we get a stat request for a file with no /? */
		strncpy(leaf, base, 128);
		strncpy(base, "./", 512);
	}

	if ( strstr(fname, "desktop.ini") ) 
//		return default_vfs_ops.stat(conn, fname, sbuf);
		return SMB_VFS_NEXT_STAT(musicdb_handle, conn, fname, sbuf);

	/* search the base */
	if ( !search(1, base, &drow, &sstack, mydata->dtable) ) {
		errno = ENOTDIR;
		return -1;
	}

	if ( drow->fileqry[0] != '\0' ) {
		replace(drow->fileqry, &sstack, filequery, mydata->sock);
		if ( stat_find_file(filequery, leaf, sbuf, conn, musicdb_handle) )
			return 0;
	}
	if ( drow->dirqry[0] != '\0' ) {
		replace(drow->dirqry, &sstack, dirquery, mydata->sock);
		if ( stat_find_dir(dirquery, leaf, sbuf, mydata->sock, conn, musicdb_handle) )
			return 0;
	}
	if ( drow->subptr > 0 ) {
		leaf[strlen(leaf)] = '/';
		if ( stat_find_sdir(leaf, drow->subptr, sbuf, mydata->dtable) )
			return 0;
	}
	
	errno = ENOENT;
	return -1;
}


static int stat_find_file(char *query, const char *leaf, SMB_STRUCT_STAT *sbuf, struct connection_struct *conn, struct vfs_handle_struct *musicdb_handle)
{
	struct song_entry *entry;
	struct cache_node *cache_entry;
	struct musicdb_ctx *mydata = (struct musicdb_ctx *)musicdb_handle->data;
	MYSQL *sock = mydata->sock;

	cache_entry = (struct cache_node *)ubi_cacheGet(mydata->cache, query);
	if ( cache_entry == NULL ) {
		/* couldn't find it in the cache. we have to load it. */
		entry = (struct song_entry *)
					cache_load(SONG, query, sock, mydata->cache);
	} else {
		/* found it set song_entry */
		entry = (struct song_entry *)
					ubi_slFirst((ubi_slListPtr)cache_entry->data);
	}

	while ( entry ) {
		if ( strcmp(entry->fname, leaf) == 0 ) {
			/* found it! stat it and return */
			memset(sbuf, 0, sizeof(SMB_STRUCT_STAT));
			sbuf->st_dev = 770;             // ??
			sbuf->st_ino = 1;               // Fake inode
			sbuf->st_mode = 0100777;
			sbuf->st_uid = 0;
			sbuf->st_gid = 0;
			sbuf->st_nlink = 1;             // number of hard links
			sbuf->st_blksize = 4096;        // Physical block size
			sbuf->st_blocks = 1;            // Number of blocks allocated
			sbuf->st_atime = 1027382400;
			sbuf->st_mtime = 1027382400;
			sbuf->st_ctime = 1027382400;
			sbuf->st_size = entry->filesize;
			return 1;
		}
		entry = (struct song_entry *)ubi_slNext(entry);
	}

	return 0;	/* not found */
}

static int stat_find_dir(const char *query, const char *leaf, SMB_STRUCT_STAT *sbuf, MYSQL *sock, struct connection_struct *conn, struct vfs_handle_struct *musicdb_handle)
{
	struct dir_entry *entry;
	struct cache_node *cache_entry;
	struct musicdb_ctx *mydata = (struct musicdb_ctx *)musicdb_handle->data;

	cache_entry = (struct cache_node *)ubi_cacheGet(mydata->cache, query);
	if ( cache_entry == NULL ) {
		/* couldn't find it in the cache. we have to load it. */
		entry = (struct dir_entry *)
					cache_load(DDIR, query, sock, mydata->cache);
	} else {
		/* found it set song_entry */
		entry = (struct dir_entry *)
					ubi_slFirst((ubi_slListPtr)cache_entry->data);
	}

	while ( entry ) {
		if ( strncmp(leaf, entry->template, 128) == 0 ) {
			generic_dir_stat(sbuf);
			return 1;
		}
		entry = (struct dir_entry *)ubi_slNext(entry);
	}

	return 0;
}

static int stat_find_sdir(const char *leaf, int subptr, SMB_STRUCT_STAT *sbuf, struct dirtbl *dtable)
{
	int i = dirtbl_search(subptr, dtable);

	if ( i == -1 ) return 0;
	while ( i < dtable->size && dtable->rows[i].dirid == subptr ) {
		if ( dtable->rows[i].visible == 'F' ) {
			i++;
			continue;			/* skip */
		}
		if ( strncmp(dtable->rows[i].template, leaf, 128) == 0 ) {
			generic_dir_stat(sbuf);
			return 1;
		}
		i++;
	}
	return 0;
}


static int musicdb_fstat(struct vfs_handle_struct *musicdb_handle, struct files_struct *fsp, int fd, SMB_STRUCT_STAT *sbuf)
{
	return module_fstat(fd, sbuf);
}


static BOOL musicdb_lock(struct vfs_handle_struct *musicdb_handle, struct files_struct *fsp, int fd, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	return 0;
}


static int check_black_list(const char *host, MYSQL *sock)
{
	MYSQL_RES *res;
	MYSQL_ROW myrow;
	char SQL[] = "SELECT * FROM blacklist";

	if ( MYSQL_QUERY(sock, SQL) ) {
		DEBUG(1, ("musicdb: failed to load black list table. Blocking ALL\n"));
		return 1;
	}
	res = mysql_store_result(sock);
	while ( (myrow = mysql_fetch_row(res)) ) {
		if ( strcasecmp(myrow[0], host) == 0 ) {
			/* found host name in list */
			mysql_free_result(res);
			return 1;
		}
	}
	mysql_free_result(res);

	return 0;
}


/**************************************************************************
 * int generic_dir_stat
 *
 * function for stat which fills in a stat structure for a directory.
 * returns 0 on success, non-zero otherwise, appropriate for stat
 * ************************************************************************/
static int generic_dir_stat(SMB_STRUCT_STAT *sbuf)
{
	memset(sbuf, 0, sizeof(SMB_STRUCT_STAT));
	sbuf->st_dev = 770;             // ??
	sbuf->st_ino = 1;               // Fake inode
	/* sbuf->st_mode = 042555; */
	/* apparently, windows spams for desktop.ini if the directory is marked
	 * read only. Go figure. we'll mark it read-write and deny access later */
	sbuf->st_mode = 042777;
	sbuf->st_uid = 0;
	sbuf->st_gid = 0;
	sbuf->st_nlink = 1;             // number of hard links
	sbuf->st_size = 4096;           // Actual size in bytes
	sbuf->st_blksize = 4096;        // Physical block size
	sbuf->st_blocks = 1;            // Number of blocks allocated
	sbuf->st_atime = 1027382400;
	sbuf->st_mtime = 1027382400;
	sbuf->st_ctime = 1027382400;

	return 0;
}


static vfs_op_tuple musicdb_ops[] = {
	{ SMB_VFS_OP(musicdb_connect),	SMB_VFS_OP_CONNECT,		SMB_VFS_LAYER_OPAQUE },
	{ SMB_VFS_OP(musicdb_disconnect),	SMB_VFS_OP_DISCONNECT,	SMB_VFS_LAYER_OPAQUE },

	{ SMB_VFS_OP(musicdb_opendir),	SMB_VFS_OP_OPENDIR,	SMB_VFS_LAYER_OPAQUE },
	{ SMB_VFS_OP(musicdb_readdir),	SMB_VFS_OP_READDIR,	SMB_VFS_LAYER_OPAQUE },
	{ SMB_VFS_OP(musicdb_closedir),	SMB_VFS_OP_CLOSEDIR,	SMB_VFS_LAYER_OPAQUE },

	{ SMB_VFS_OP(musicdb_stat),	SMB_VFS_OP_STAT,	SMB_VFS_LAYER_OPAQUE },

	{ SMB_VFS_OP(musicdb_open),	SMB_VFS_OP_OPEN,	SMB_VFS_LAYER_OPAQUE },
	{ SMB_VFS_OP(musicdb_close),	SMB_VFS_OP_CLOSE,	SMB_VFS_LAYER_OPAQUE },
	{ SMB_VFS_OP(musicdb_read),	SMB_VFS_OP_READ,	SMB_VFS_LAYER_OPAQUE },
	{ SMB_VFS_OP(musicdb_fstat),	SMB_VFS_OP_FSTAT,	SMB_VFS_LAYER_OPAQUE },
	{ SMB_VFS_OP(musicdb_lseek),	SMB_VFS_OP_LSEEK,	SMB_VFS_LAYER_OPAQUE },
	{ SMB_VFS_OP(musicdb_lock),		SMB_VFS_OP_LOCK,	SMB_VFS_LAYER_OPAQUE },

	{ SMB_VFS_OP(NULL), SMB_VFS_OP_NOOP, SMB_VFS_LAYER_NOOP },
};


NTSTATUS init_module(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "databasefs", musicdb_ops);
}
