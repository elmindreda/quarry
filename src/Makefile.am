# Process this file with Automake to produce `Makefile.in'.

SUBDIRS =		\
	utils		\
	board		\
	sgf		\
	gtp		\
	gui-utils	\
	gui-gtk

AM_CPPFLAGS =					\
	-D LOCALE_DIR=\"$(datadir)/locale\"	\
						\
	$(QUARRY_WARNINGS)			\
						\
	-I $(top_srcdir)/src/gui-gtk		\
	-I $(top_srcdir)/src/gui-utils		\
	-I $(top_srcdir)/src/board		\
	-I $(top_srcdir)/src/utils


bin_PROGRAMS = quarry

quarry_SOURCES =	\
	quarry.c	\
			\
	gettext.h	\
	quarry.h

quarry_LDADD = 			\
	gui-gtk/libguigtk.a	\
	gui-utils/libguiutils.a	\
	gtp/libgtp.a		\
	sgf/libsgf.a		\
	board/libboard.a	\
	utils/libutils.a	\
				\
	$(QUARRY_GTK_LIBS)	\
	$(QUARRY_GTHREAD_LIBS)


DISTCLEANFILES = *~
