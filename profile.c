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

static int xmallocs = 0;

void *MALLOC(size_t size)
{
	xmallocs++;
	return malloc(size);
}

void STRDUP() { xmallocs++; }

void FREE(void *ptr)
{
	xmallocs--;
	free(ptr);
}

void report_mem_usage(const char *tracking)
{
	DEBUG(1, ("musicdb: mem_usage(%s): %d\n", tracking, xmallocs));
}
