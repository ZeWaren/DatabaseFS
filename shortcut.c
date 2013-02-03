/*************************************************************************
 * MusicDB - Copyright Eric Lorimer (5/02)
 *
 * This file holds the functions related to creating Windows shortcut
 * files dynamically.
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
#include "shortcut.h"


/***************************************************************************
 * int load_shortcut_template
 *
 * loads a shortcut template from a given filename.  We need this as I
 * currently don't fully understand the shortcut format, so I copy.
 * *************************************************************************/
int load_shortcut_template(const char *fname, struct shortcut *cut)
{
	FILE *in;
	int size;
	char buffer[512];

	if ( (in = fopen(fname, "r")) == NULL ) {
		//DEBUG(2, ("musicdb: failed to load shortcut template %s\n", fname));
		return -1;
	}
	size = fread(buffer, 1, 512, in);
	if ( !feof(in) ) {
		/* buffer too small; we should really try again here */
		return -2;
	}
	/* we really only need the garbage section */
	memcpy(cut->garbage, buffer, 200);

	return 0;
}


/***************************************************************************
 * int make_shortcut
 *
 * fills in buffer with the shortcut data in scut
 * *************************************************************************/
int make_shortcut(char *buffer, int bufsize, struct shortcut *scut)
{
	int bytesw = 0, sectionlen, i;
	char *start = buffer;
	int (*sectiontbl[])(char *, int, struct shortcut *) = {
		make_shortcut_hostsection,
		make_shortcut_sharesection,
		make_shortcut_filesection,
		make_shortcut_pathsection,
		NULL, };

	if ( bufsize < 400 ) return 0; /* buffer too small; yell */
	memcpy(buffer, scut->garbage, 200);
	bytesw += 200;
	buffer += 200;

	for (i=0; sectiontbl[i] != NULL; i++) {
		sectionlen = (*sectiontbl[i])(buffer, bufsize - bytesw, scut);
		if ( sectionlen == 0 ) return 0;		/* buffer too small */
		bytesw += sectionlen;
		buffer += sectionlen;
	}

	if ( bytesw + scut->unicodelen*2+7 > bufsize ) return 0;
	buffer += 2;
	sectionlen = scut->unicodelen;
	*(short int *)buffer = sectionlen;
	buffer += 2;
	for (i=0; i < scut->unicodelen*2+1; i++)
		*buffer++ = scut->unicodedata[i];
	memset(buffer, 0x00, 3);

	*(short int *)(start + 76) = strlen(scut->sharedata[0]) * 2 
		+ strlen(scut->sharedata[1]) + strlen(scut->sharedata[2]) 
		+ strlen(scut->filedata[0]) + strlen(scut->filedata[1]) + 158;
	return (buffer - start + 3);
}


/***************************************************************************
 * int make_shortcut_hostsection
 *
 * helper function for make_shortcut to fill in the host section
 * *************************************************************************/
int make_shortcut_hostsection(char *buffer, int bufsize, struct shortcut *scut)
{
	int sectionlen;

	sectionlen = strlen(scut->hostdata[0]) + strlen(scut->hostdata[1]) + 9;
	if ( sectionlen > bufsize ) return 0;		/* buffer too small */

	*(short int *)buffer = sectionlen;
	buffer += 2;
	*(short int *)buffer = 66;			/* no idea */
	buffer += 2;
	*buffer++ = 130;						/* again, I'm just copying this */
	memcpy(buffer, scut->hostdata[0], strlen(scut->hostdata[0])+1);
	buffer += strlen(scut->hostdata[0]) + 1;
	memcpy(buffer, scut->hostdata[1], strlen(scut->hostdata[1])+1);
	buffer += strlen(scut->hostdata[1]) + 1;
	*(short int *)buffer = 2;
	buffer += 2;

	return sectionlen;
}


/***************************************************************************
 * int make_shortcut_sharesection
 *
 * helper function for make_shortcut to fill in the share section
 * *************************************************************************/
int make_shortcut_sharesection(char *buffer, int bufsize, struct shortcut *scut)
{
	int sectionlen;

	sectionlen = strlen(scut->sharedata[0]) + strlen(scut->sharedata[1]) +
				strlen(scut->sharedata[2]) + 10;
	if ( sectionlen > bufsize ) return 0;		/* buffer too small */
	*(short int *)buffer = sectionlen;
	buffer += 2;
	*buffer++ = 0xc3;
	*buffer++ = 0x01;
	*buffer++ = 0xc5;
	memcpy(buffer, scut->sharedata[0], strlen(scut->sharedata[0])+1);
	buffer += strlen(scut->sharedata[0]) + 1;
	memcpy(buffer, scut->sharedata[1], strlen(scut->sharedata[1])+1);
	buffer += strlen(scut->sharedata[1]) + 1;
	memcpy(buffer, scut->sharedata[2], strlen(scut->sharedata[2])+1);
	buffer += strlen(scut->sharedata[2]) + 1;
	*(short int *)buffer = 2;
	buffer += 2;

	return sectionlen;
}


/***************************************************************************
 * int make_shortcut_filesection
 *
 * helper function for make_shortcut to fill in the file section
 * *************************************************************************/
int make_shortcut_filesection(char *buffer, int bufsize, struct shortcut *scut)
{
	int sectionlen;
	char moremagic[] = {
		0x32, 0x00, 0x03, 0xD0, 0x42, 0x00, 0xAD, 0x2C,
		0xE6, 0x8D, 0x80, 0x00 };

	sectionlen = strlen(scut->filedata[0]) + strlen(scut->filedata[1]) + 16;
	if ( sectionlen > bufsize ) return 0;
	*(short int *)buffer = sectionlen;
	buffer += 2;
	memcpy(buffer, moremagic, 12);
	buffer += 12;
	memcpy(buffer, scut->filedata[0], strlen(scut->filedata[0])+1);
	buffer += strlen(scut->filedata[0]) + 1;
	memcpy(buffer, scut->filedata[1], strlen(scut->filedata[1])+1);
	buffer += strlen(scut->filedata[1]) + 1;
	*(short int *)buffer = 0x00;

	return sectionlen;
}


/***************************************************************************
 * int make_shortcut_pathsection
 *
 * helper function for make_shortcut to fill in the path section
 * *************************************************************************/
int make_shortcut_pathsection(char *buffer, int bufsize, struct shortcut *scut)
{
	int sectionlen;
	char magic[] = {
		0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
		0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x02, 0x00, };

	sectionlen = strlen(scut->pathdata[0]) + strlen(scut->pathdata[1]) + 50;
	if ( sectionlen > bufsize ) return 0;
	buffer += 2;
	*(short int *)buffer = sectionlen;
	buffer += 1;
	memcpy(buffer, magic, sizeof(magic));
	buffer += sizeof(magic);
	memcpy(buffer, scut->pathdata[0], strlen(scut->pathdata[0])+1);
	buffer += strlen(scut->pathdata[0])+1;
	memcpy(buffer, scut->pathdata[1], strlen(scut->pathdata[1])+1);
	buffer += strlen(scut->pathdata[1])+1;

	return sectionlen;
}


/***************************************************************************
 * int easy_shortcut
 *
 * high level function to create a shortcut
 * *************************************************************************/
int easy_shortcut(const char *host, const char *share, const char *path, struct shortcut *template, char *buffer, int bufsize)
{
	int i, size;
	char *myhost = (char *)MALLOC(strlen(host) + 3);
	char *sharedata = (char *)MALLOC(strlen(host) + strlen(share) + 4);

	sprintf(myhost, "\\\\%s", host);
	sprintf(sharedata, "\\\\%s\\%s", host, share);
	template->hostdata[0] = myhost;
	template->hostdata[1] = share;
	template->sharedata[0] = sharedata;
	template->sharedata[1] = "Microsoft Network";
	template->sharedata[2] = "some bogus share";
	template->filedata[0] = path;
	template->filedata[1] = "ABCDEF~1.MP3";
	template->pathdata[0] = "";
	template->pathdata[1] = path;
	template->unicodelen = strlen(sharedata);
	template->unicodedata = (char *)MALLOC(template->unicodelen * 2 + 16);
	memset(template->unicodedata, 0x00, template->unicodelen * 2);
	for (i = 0; i < strlen(sharedata) + 1; i++)
		template->unicodedata[i * 2] = sharedata[i];
	size = make_shortcut(buffer, bufsize, template);
	FREE(template->unicodedata);
	FREE(myhost);
	FREE(sharedata);

	return size;
}
