# Process this file with Automake to produce `Makefile.in'.

QUARRY_FULL_GTK_CFLAGS = 		\
	$(QUARRY_GTK_CFLAGS)		\
	$(QUARRY_GTK_DEPRECATED_FLAGS)	\
	$(QUARRY_GTHREAD_CFLAGS)

I_CPPFLAGS =				\
	-I $(top_srcdir)/src		\
	-I $(top_srcdir)/src/board	\
	-I $(top_srcdir)/src/gtp	\
	-I $(top_srcdir)/src/gui-utils	\
	-I $(top_srcdir)/src/sgf	\
	-I $(top_srcdir)/src/utils

AM_CPPFLAGS = 							\
	-D PACKAGE_DATA_DIR=\"$(datadir)/quarry\"		\
								\
	$(QUARRY_WARNINGS_GTK)					\
								\
	$(I_CPPFLAGS)						\
								\
	$(QUARRY_FULL_GTK_CFLAGS)


noinst_LIBRARIES = libguigtk.a

BUILT_SOURCES =			\
	gtk-configuration.c	\
	quarry-marshal.c	\
				\
	gtk-configuration.h	\
	quarry-marshal.h


quarry-marshal.c : quarry-marshal.list
	@GLIB_GENMARSHAL@ quarry-marshal.list --stdinc	\
			  --prefix=quarry_marshal	\
			  --body > quarry-marshal.c	\
	  || (rm -f quarry-marshal.c ; exit 1)

quarry-marshal.h : quarry-marshal.list
	@GLIB_GENMARSHAL@ quarry-marshal.list --stdinc	\
			  --prefix=quarry_marshal	\
			  --header > quarry-marshal.h	\
	  || (rm -f quarry-marshal.h ; exit 1)

# FIXME: find a nicer way to avoid warnings in `quarry-marshal.c'
#	 (missing prototypes are easy; the trick is to cope with
#	 unused parameters).
quarry-marshal.$(OBJEXT) : quarry-marshal.c quarry-marshal.h
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES)	\
	      $(QUARRY_FULL_GTK_CFLAGS)			\
	      $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)	\
	      -o quarry-marshal.$(OBJEXT)		\
	      -c quarry-marshal.c			\
	  || (rm -f quarry-marshal.$(OBJEXT) ; exit 1)


PARSE_CONFIGURATION =							\
	$(top_srcdir)/src/gui-utils/parse-configuration$(EXEEXT)

gtk-configuration.c gtk-configuration.h					\
  : gtk-configuration.list						\
    $(top_srcdir)/src/gui-utils/common-configuration-sections.list	\
    $(top_srcdir)/src/gui-utils/common-configuration-values.list	\
    $(top_srcdir)/src/gui-utils/common-configuration-defaults.list	\
    $(PARSE_CONFIGURATION)
	$(PARSE_CONFIGURATION)						\
	    -D PACKAGE_TEXTURES_DIR="\"$(datadir)/quarry/textures\""	\
	    gtk-configuration.list					\
	    gtk-configuration.h gtk-configuration.c			\
	  || (rm -f gtk-configuration.c gtk-configuration.h ; exit 1)


libguigtk_a_SOURCES =			\
	gtk-assistant.c			\
	gtk-clock.c			\
	gtk-color-button.c		\
	gtk-control-center.c		\
	gtk-games.c			\
	gtk-goban.c			\
	gtk-goban-window.c		\
	gtk-gtp-client-interface.c	\
	gtk-named-vbox.c		\
	gtk-new-game-dialog.c		\
	gtk-parser-interface.c		\
	gtk-preferences.c		\
	gtk-progress-dialog.c		\
	gtk-qbox.c			\
	gtk-qhbox.c			\
	gtk-qvbox.c			\
	gtk-tile-set.c			\
	gtk-utils.c			\
	gui-back-end.c			\
	quarry-stock.c			\
					\
	gtk-assistant.h			\
	gtk-clock.h			\
	gtk-color-button.h		\
	gtk-control-center.h		\
	gtk-games.h			\
	gtk-goban.h			\
	gtk-goban-window.h		\
	gtk-gtp-client-interface.h	\
	gtk-named-vbox.h		\
	gtk-new-game-dialog.h		\
	gtk-parser-interface.h		\
	gtk-preferences.h		\
	gtk-progress-dialog.h		\
	gtk-qbox.h			\
	gtk-qhbox.h			\
	gtk-qvbox.h			\
	gtk-thread-interface.h		\
	gtk-tile-set.h			\
	gtk-utils.h			\
	gui-back-end.h			\
	quarry-stock.h

nodist_libguigtk_a_SOURCES = $(BUILT_SOURCES)


# This seems to be the best way of solving circular references.
libguigtk_a_LIBADD = $(top_srcdir)/src/gui-utils/libguiutils.a


EXTRA_DIST =			\
	gtk-configuration.list	\
	quarry-marshal.list

DISTCLEANFILES = *~

MOSTLYCLEANFILES = $(BUILT_SOURCES)
