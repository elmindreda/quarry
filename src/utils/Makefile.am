# Process this file with Automake to produce `Makefile.in'.

AM_CPPFLAGS =			\
	$(QUARRY_WARNINGS)	\
				\
	-I $(top_srcdir)/src


noinst_LIBRARIES =	\
	libparselist.a	\
	libutils.a

libutils_a_SOURCES =		\
	buffered-writer.c	\
	format.c		\
	getopt.c		\
	getopt1.c		\
	memory-pool.c		\
	string-buffer.c		\
	string-list.c		\
	utils.c			\
				\
	getopt.h		\
	utils.h

libparselist_a_SOURCES =	\
	parse-list.c		\
				\
	parse-list.h


DISTCLEANFILES = *~
