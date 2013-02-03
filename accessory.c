/*************************************************************************
 * MusicDB - Copyright Eric Lorimer (5/02)
 *
 * This file hold accessory functions related to MusicDB.
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
**************************************************************************/
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
#include "ubi_sLinkList.h"

#define DIRTBL_SQL		"SELECT * FROM dirtbl WHERE uname=\"%s\" ORDER BY dirid, priority"
extern userdom_struct current_user_info;


/***************************************************************************
 * int replace
 *
 * fill in the template given by source using the stack and return it into
 * dest
 * *************************************************************************/
int replace(const char *source, struct strstack *stack, pstring dest, MYSQL *mysql)
{
	char *uname = current_user_info.smb_name;
	const char *remote;
	enum { COPY, PARAM } state = COPY;
	char buffer[6];
	int i, cur_parm, bindex = 0;
	fstring replaced;
	char replaced2[512];

	for (i=0; i < strlen(source); i++) {
		switch (state) {
			case COPY:
				if ( source[i] != '$' )
					*dest++ = source[i];
				else state = PARAM;
				break;
			case PARAM:
				if ( source[i] > '9' || source[i] < '0' ) {
					buffer[bindex] = '\0';
					if ( buffer[0] == '\0' ) {
						switch (source[i]) {
							case 'U':		/* user name */
								pstrcpy(dest, uname);
								dest += strlen(uname);
								break;
							case 'I':		/* remote host ip address */
								pstrcpy(dest, client_name());
								dest += strlen(client_name());
								break;
							case 'N':		/* host name */
								remote = get_remote_machine_name();
								pstrcpy(dest, remote);
								dest += strlen(remote);
								break;
							default:
								*dest++ = source[i];
								break;
						}
					} else {
						cur_parm = atoi(buffer);
						stack_get(stack, cur_parm, replaced);
						mysql_real_escape_string(mysql, replaced2, replaced,
								strlen(replaced));
						pstrcpy(dest, replaced2);
						dest += strlen(replaced2);
						*dest++ = source[i];
					}
					bindex = 0;
					state = COPY;
				} else
					buffer[bindex++] = source[i];
				break;
		}
	}
	*dest = '\0';

	return 1;
}


/**************************************************************************
 * int template_match
 *
 * try to match a directory path against a template.
 * ************************************************************************/
int template_match(const char *template, const char *dpath, fstring buffer)
{
	int i;
	int len = (strlen(dpath) > 256) ? 256 : strlen(dpath);

	for (i=0; i < len && template[i] == dpath[i]; i++)
		*buffer++ = dpath[i];
	if ( template[i] == '%' ) {
		char stopch = template[i+1];
		for (; i < len && dpath[i] != stopch; i++)
			*buffer++ = dpath[i];
		if ( dpath[i] == stopch ) i++;
	} else
		buffer--;
	*buffer = '\0';

	return i;
}


/**************************************************************************
 * int search
 *
 * recursive function to search for a directory. uses the global dtable
 * ************************************************************************/
int search(int dirid, const char *dpath, struct dirtbl_row **drow, struct strstack *stack, struct dirtbl *dtable)
{
	fstring matchstr;
	int matched = 0, i;

	i = dirtbl_search(dirid, dtable);
	if ( i == -1 )
		return 0;

	while ( i < dtable->size && dtable->rows[i].dirid == dirid ) {
		int nmatched = template_match(dtable->rows[i].template, dpath,
				matchstr);
		if ( nmatched >= strlen(dtable->rows[i].template) ) {		// FIX
			stack_push(stack, matchstr);
			matched = 1;
			if ( dtable->rows[i].subptr != -1 ) {
				dpath += nmatched;
				if ( *dpath )
					return search(dtable->rows[i].subptr, dpath, drow, stack, dtable);
				*drow = &dtable->rows[i];
				return 1;
			} else {
				if ( nmatched == strlen(dpath) ) {
					*drow = &dtable->rows[i];
					return 1;
				}
				return 0;
			}
		}
		i++;
	}

	return 0;
}


/**************************************************************************
 * int load_dirtbl
 *
 * load the entire dirtbl into memory to speed up searches
 * ************************************************************************/
int load_dirtbl(MYSQL *sock, struct dirtbl *dtable)
{
	MYSQL_RES *res;
	MYSQL_ROW myrow;
	int i = 0;
	struct dirtbl_row *dirtbl = dtable->rows;
	char SQL[256];
	
	snprintf(SQL, 256, DIRTBL_SQL, current_user_info.smb_name);
	if ( MYSQL_QUERY(sock, SQL) ) {
		DEBUG(3, ("MYSQL_QUERY failed loading dirtbl: %s\n", SQL));
		return 0;
	}
	res = mysql_store_result(sock);

	if ( !res || (mysql_num_rows(res) == 0) ) {
		/* no such user; fall back to the defaults */
		snprintf(SQL, 256, DIRTBL_SQL, "*");
		if ( MYSQL_QUERY(sock, SQL) ) {
			DEBUG(3, ("MYSQL_QUERY failed loading dirtbl: %s\n", SQL));
			return 0;
		}
		res = mysql_store_result(sock);
	}
	if ( ! res ) {
		DEBUG(3, ("musicdb could not load dirtbl defaults.\n"));
		return 0;
	}

	while ( (myrow = mysql_fetch_row(res)) && i < dtable->size ) {
		dirtbl[i].dirid = atoi(myrow[1]);
		if ( myrow[2] ) strncpy(dirtbl[i].template, myrow[2], 128);
			else dirtbl[i].template[0] = '\0';
		if ( myrow[3] ) dirtbl[i].subptr = atoi(myrow[3]);
			else dirtbl[i].subptr = -1;
		if ( myrow[4] ) strncpy(dirtbl[i].fileqry, myrow[4], 1024);
			else dirtbl[i].fileqry[0] = '\0';
		if ( myrow[5] ) strncpy(dirtbl[i].dirqry, myrow[5], 1024);
			else dirtbl[i].dirqry[0] = '\0';
		if ( myrow[7] ) dirtbl[i].visible = myrow[7][0];
			else dirtbl[i].visible = 'F';
		i++;
	}
	mysql_free_result(res);

	return i;
}


/**************************************************************************
 * int dirtbl_search
 *
 * search for the given dirid from the dtable. Linear search. Easy to make
 * binary search for better performance, but we don't have enough directory
 * entries to make it worth it.
 * ************************************************************************/
int dirtbl_search(int dirid, struct dirtbl *dtable)
{
	/* for now we'll do a linear search. Implement binary search later */
	int i;

	for (i=0; i < dtable->size; i++)
		if ( dtable->rows[i].dirid == dirid )
			return i;
	return -1;
}


/**************************************************************************
 * int MYSQL_QUERY
 *
 * wrapper around mysql_query to do some basic profiling
 * ************************************************************************/
int MYSQL_QUERY(MYSQL *sock, const char *query)
{
	char SQL[1024];
	char *uname = current_user_info.smb_name;

	DEBUG(3, ("MYSQL_QUERY: %s\n", query));
	snprintf(SQL, 1024, "INSERT INTO logs VALUES ('%s', NOW(), '%s')", uname,
			query);
	mysql_query(sock, SQL);
	return mysql_query(sock, query);
}


/**************************************************************************
 * int compare_nodes
 *
 * function used by the cache to compare items and keys in nodes
 * ************************************************************************/
int compare_nodes(ubi_btItemPtr item, ubi_btNodePtr node)
{
	return strcmp((char *)item, ((struct cache_node *)node)->key);
}


/**************************************************************************
 * void killnode
 *
 * function used by the cache to cleanup cache nodes. Goes through the
 * linked list and deletes each entry.
 * ************************************************************************/
void killnode(ubi_trNodePtr thenode)
{
	struct cache_node *node = (struct cache_node *)thenode;
	struct dir_entry *dnode;
	struct song_entry *snode;
	int i;
	
	/* go through the linked list and free all memory */
	switch (node->type) {
		case DDIR:
			while ( (dnode = (struct dir_entry *)ubi_slRemHead(node->data)) ) {
				FREE(dnode->template);
				FREE(dnode);
			}
			break;
		case SONG:
			while ( (snode = (struct song_entry *)ubi_slRemHead(node->data)) ) {
				for (i=0; i < snode->nfields; i++)
					FREE(snode->fields[i]);
				FREE(snode);
			}
			break;
	}
	FREE(node->data);
	FREE(node->key);
	FREE(node);

	return;
}


/**************************************************************************
 * ubi_slNodePtr cache_load
 *
 * load the specified query and add it to the cache. return a pointer to
 * the linked-list of data
 * ************************************************************************/
ubi_slNodePtr cache_load(cachetype type, char *query, MYSQL *sock,
	ubi_cacheRoot *cache)
{
	int count;
	ubi_slListPtr newlist;
	struct cache_node *cache_entry;

	newlist = (ubi_slListPtr)MALLOC(sizeof(ubi_slList));

	switch (type) {
		case SONG:
			count = load_song_query(newlist, query, sock);
			break;
		default:
			count = load_dir_query(newlist, query, sock);
	}

	if ( count == 0 ) {
		FREE(newlist);
		return NULL;
	}

	/* and add it to the cache */
	cache_entry = (struct cache_node *)MALLOC(sizeof(struct cache_node));
	cache_entry->type = type;
	cache_entry->key = strdup(query);
	STRDUP();
	cache_entry->data = newlist;
	ubi_cachePut(cache, count, (ubi_cacheEntryPtr)cache_entry,
		(ubi_trItemPtr)cache_entry->key);

	return ubi_slFirst(newlist);
}
