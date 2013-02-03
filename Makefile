#
# Makefile for samba-vfs examples
#
#

# Variables

CC = gcc
LIBTOOL = libtool

SAMBA_SRC = ../../../source
SAMBA_INCL = ../../../source/include
POPT_INCL = ../../../source/popt
MYSQL_INCL = /usr/local/mysql/include/mysql
UBIQX_SRC = ../../../source/ubiqx
SMBWR_SRC = ../../../source/smbwrapper
KRB5_SRC = /usr/kerberos/include
CFLAGS = -I$(SAMBA_SRC) -I$(SAMBA_INCL) -I$(POPT_INCL) -I$(UBIQX_SRC) -I$(SMBWR_SRC) -I$(KRB5_SRC) -I$(MYSQL_INCL) -Wall -g
LIBS = /usr/local/mysql/lib/mysql/libmysqlclient.so
OBJS = stack.o accessory.o songlist.o shortcut.o config.o profile.o \
	module.o
VFS_OBJS = databasefs.so stack.o accessory.o songlist.o shortcut.o config.o \
	profile.o module.o
# Default target

default: $(VFS_OBJS)

# Pattern rules

%.so: %.lo $(OBJS)
	$(LIBTOOL) $(CC) -shared -o $@ $< $(LIBS) $(OBJS) $(LDFLAGS)

%.lo: %.c
	$(LIBTOOL) $(CC) $(CPPFLAGS) $(CFLAGS) -c $<

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<


# Misc targets

clean:
	rm -rf .libs
	rm -f core *~ *% *.bak \
		$(VFS_OBJS) $(VFS_OBJS:.so=.o) $(VFS_OBJS:.so=.lo) 
