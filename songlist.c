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
*************************************************************************/
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


/***************************************************************************
 * int load_song_query
 *
 * initializes and populates a linked list with file query data.
 * *************************************************************************/
int load_song_query(ubi_slListPtr songlist, const char *query, MYSQL *sock)
{
	MYSQL_RES *res;
	MYSQL_ROW myrow;
	int count = 0, i, num_fields;
	struct song_entry *node;
	unsigned long *lengths;

	ubi_slInitList(songlist);
	
	if ( MYSQL_QUERY(sock, query) ) {
		DEBUG(1, ("musicdb: MYSQL_QUERY failed: %s\n", query));
		return 0;
	}
	res = mysql_store_result(sock);
	if ( ! res ) {
		DEBUG(1, ("musicdb: MYSQL_QUERY failed. res = NULL: %s\n", query));
		return 0;
	}
	while ( (myrow = mysql_fetch_row(res)) ) {
		node = (struct song_entry *)MALLOC(sizeof(struct song_entry));
		strncpy(node->fname, myrow[0], 128);
		node->filesize = atoi(myrow[1]);
		lengths = mysql_fetch_lengths(res);
		num_fields = mysql_num_fields(res);
		for (i=0; i < 16 && i < num_fields-2; i++) {
//			node->fields[i] = (char *)MALLOC(512);  // lengths[i+2]);
			node->fields[i] = (char *)MALLOC(lengths[i+2]+1);
			memcpy(node->fields[i], myrow[i+2], lengths[i+2]);
			node->fields[i][lengths[i+2]] = '\0';
		}
		node->nfields = i;
		ubi_slAddHead(songlist, node);
		count++;
	}
	mysql_free_result(res);

	return count;
}


/***************************************************************************
 * int load_dir_query
 *
 * initializes and populates a linked list by running the query. we could
 * probably combine this with load_song_query, but makes more sense to
 * keep them separate.
 * *************************************************************************/
int load_dir_query(ubi_slListPtr dirlist, const char *query, MYSQL *sock)
{
	MYSQL_RES *res;
	MYSQL_ROW myrow;
	int count = 0;
	struct dir_entry *node;

	ubi_slInitList(dirlist);
	
	if ( MYSQL_QUERY(sock, query) ) {
		DEBUG(1, ("musicdb: MYSQL_QUERY failed: %s\n", query));
		return 0;
	}
	res = mysql_store_result(sock);
	while ( (myrow = mysql_fetch_row(res)) ) {
		node = (struct dir_entry *)MALLOC(sizeof(struct dir_entry));
		node->template = strdup(myrow[0]);
		STRDUP();
		ubi_slAddHead(dirlist, node);
		count++;
	}
	mysql_free_result(res);

	return count;
}
