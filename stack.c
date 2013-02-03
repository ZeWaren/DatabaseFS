/************************************************************************
 * MusicDB - Copyright Eric Lorimer (5/02)
 *
 * This file holds accessory functions related to MusicDB. All right,
 * this is really a useless piece of code and I apologize.  This is not 
 * a stack, I know.
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
************************************************************************/
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


/**************************************************************************
 * void stack_push
 *
 * push a string onto the stack.
 * ************************************************************************/
void stack_push(struct strstack *stack, fstring item)
{
	if ( stack->stackptr + 1 > 32 )
		return;				// no more space. ignore
//	pstrcpy(stack->stack[stack->stackptr++], item);
	strncpy(stack->stack[stack->stackptr++], item, FSTRING_LEN);
}


void stack_get(const struct strstack *stack, int index, fstring buffer)
{
	if ( index > -1 && index < stack->stackptr )
//		pstrcpy(buffer, stack->stack[index]);
		strncpy(buffer, stack->stack[index], FSTRING_LEN);
	else
		buffer[0] = '\0';
}
