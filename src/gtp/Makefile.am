# Process this file with Automake to produce `Makefile.in'.

AM_CPPFLAGS =				\
	$(QUARRY_WARNINGS)		\
					\
	-I $(top_srcdir)/src		\
	-I $(top_srcdir)/src/board	\
	-I $(top_srcdir)/src/sgf	\
	-I $(top_srcdir)/src/utils


noinst_LIBRARIES = libgtp.a

libgtp_a_SOURCES =	\
	gtp-client.c	\
			\
	gtp-client.h


EXTRA_PROGRAMS = gtp-test

gtp_test_SOURCES = gtp-test.c

gtp_test_LDADD =				\
	libgtp.a				\
	$(top_srcdir)/src/board/libboard.a	\
	$(top_srcdir)/src/utils/libutils.a


DISTCLEANFILES = *~

MOSTLYCLEANFILES = $(EXTRA_PROGRAMS)
