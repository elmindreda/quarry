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
	-I $(top_builddir)/src/board		\
	-I $(top_srcdir)/src/utils


bin_PROGRAMS = quarry

quarry_SOURCES =	\
	quarry.c	\
			\
	gettext.h	\
	quarry.h	\
	quarry-main.h

quarry_LDADD = 						\
	$(top_builddir)/src/gui-gtk/libguigtk.a		\
	$(top_builddir)/src/gui-utils/libguiutils.a	\
	$(top_builddir)/src/gtp/libgtp.a		\
	$(top_builddir)/src/sgf/libsgf.a		\
	$(top_builddir)/src/board/libboard.a		\
	$(top_builddir)/src/utils/libutils.a		\
							\
	$(QUARRY_GTK_LIBS)				\
	$(QUARRY_GTHREAD_LIBS)


DISTCLEANFILES = *~
