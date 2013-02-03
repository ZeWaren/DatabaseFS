/*************************************************************************
 * MusicDB - Copyright Eric Lorimer (7/02)
 *
 * This file hold accessory functions related to MusicDB for parsing the
 * config file
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
#include <stdio.h>
#include <string.h>

static struct {
	char *option;
	enum { TYPE_STRING, TYPE_INT } type;
	char *defaultval;
	char *value;
} parse_table[] = {
	{ "database",	TYPE_STRING,	"music",	NULL },
	{ "user",		TYPE_STRING,	"samba",	NULL },
	{ "password",	TYPE_STRING,	"mus1cd6",	NULL },
	{ "prefix",		TYPE_STRING,	"/home/music",	NULL },
	{ "template",	TYPE_STRING,	"template.lnk",	NULL },
	{ NULL,			TYPE_INT,		NULL,	 NULL },
};


int parse_config(const char *fname)
{
	FILE *in;
	char line[512];

	if ( (in = fopen(fname, "r")) == NULL ) {
/*		DEBUG(1, ("musicdb: unable to open config file: %s\n", fname)); */
		return 1;
	}

	while ( fgets(line, 512, in) ) {
		int i, j;
		char *ptr, *p2;
		for (i=0; i < 512 && (line[i] == ' ' || line[i] == '\t'); i++)
			;
		if ( line[i] == '\n' || line[i] == ';' || line[i] == '#' )
			continue;

		for (ptr = &line[i]; *ptr && *ptr != '='; ptr++)
			;
		*ptr = '\0';
		/* remove trailing whitespace */
		for (p2 = ptr-1; *p2 && (*p2 == ' ' || *p2 == '\t'); *p2-- = '\0')
			;
		ptr++;
		while (*ptr == ' ' || *ptr == '\t') *ptr++ = '\0';
		ptr[strlen(ptr)-1] = '\0';

		for (j = 0; parse_table[j].option; j++) {
			if ( strcmp(parse_table[j].option, &line[i]) == 0 ) {
				/* some data conversion here */
				if ( parse_table[j].value == NULL ) {
					parse_table[j].value = strdup(ptr);
					STRDUP();
				}
				else {
					FREE(parse_table[j].value);
					parse_table[j].value = NULL;
				}
			}
		}
	}

	fclose(in);
	return 0;
}


char *get_string_option(const char *option)
{
	int i;

	for (i=0; parse_table[i].option; i++) {
		if ( strcmp(parse_table[i].option, option) == 0 )
			return ( parse_table[i].value == NULL )
				? parse_table[i].defaultval
				: parse_table[i].value;
	}

	return NULL;
}


void cleanup_config()
{
	int i;
	for (i = 0; parse_table[i].option; i++)
		if ( *(char *)parse_table[i].value != NULL )
			FREE(parse_table[i].value);
}
