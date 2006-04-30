# Process this file with Automake to produce `Makefile.in'.

QUARRY_FULL_GTK_CFLAGS = 		\
	$(QUARRY_GTK_CFLAGS)		\
	$(QUARRY_GTK_DEPRECATED_FLAGS)	\
	$(QUARRY_GTHREAD_CFLAGS)

AM_CPPFLAGS = 					\
	-D PACKAGE_DATA_DIR=\"$(pkgdatadir)\"	\
						\
	$(QUARRY_WARNINGS_GTK)			\
						\
	-I $(top_srcdir)/src			\
	-I $(top_srcdir)/src/board		\
	-I $(top_builddir)/src/board		\
	-I $(top_srcdir)/src/gtp		\
	-I $(top_srcdir)/src/gui-utils		\
	-I $(top_builddir)/src/gui-utils	\
	-I $(top_srcdir)/src/sgf		\
	-I $(top_builddir)/src/sgf		\
	-I $(top_srcdir)/src/utils		\
						\
	$(QUARRY_FULL_GTK_CFLAGS)


noinst_LIBRARIES = libguigtk.a


LIST_FILES = gtk-configuration.list

PARSE_LIST_COMMAND =							\
	$(top_builddir)/src/gui-utils/parse-configuration$(EXEEXT)

PARSE_LIST_FLAGS =						\
	-D top_srcdir="$(top_srcdir)"				\
	-D PACKAGE_TEXTURES_DIR="$(pkgdatadir)/textures"

# Avoid problems with parallel builds.
gtk-configuration.c: gtk-configuration.h

include $(top_srcdir)/build/list.make


quarry-marshal.c : $(srcdir)/quarry-marshal.glist
	$(GLIB_GENMARSHAL) --stdinc --prefix=quarry_marshal --body	\
			   `test -f '$<' || echo '$(srcdir)/'`$< > $@	\
	|| (rm -f $@; exit 1)

quarry-marshal.h : $(srcdir)/quarry-marshal.glist
	$(GLIB_GENMARSHAL) --stdinc --prefix=quarry_marshal --header	\
			   `test -f '$<' || echo '$(srcdir)/'`$< > $@	\
	|| (rm -f $@; exit 1)

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


libguigtk_a_SOURCES =				\
	gtk-add-or-edit-label-dialog.c		\
	gtk-assistant.c				\
	gtk-clock.c				\
	gtk-color-button.c			\
	gtk-control-center.c			\
	gtk-file-dialog.c			\
	gtk-file-selector.c			\
	gtk-freezable-spin-button.c		\
	gtk-game-info-dialog.c			\
	gtk-games.c				\
	gtk-goban.c				\
	gtk-goban-base.c			\
	gtk-goban-window.c			\
	gtk-go-to-named-node-dialog.c		\
	gtk-gtp-client-interface.c		\
	gtk-help.c				\
	gtk-named-vbox.c			\
	gtk-new-game-dialog.c			\
	gtk-new-game-record-dialog.c		\
	gtk-parser-interface.c			\
	gtk-preferences.c			\
	gtk-progress-dialog.c			\
	gtk-qbox.c				\
	gtk-qhbox.c				\
	gtk-qvbox.c				\
	gtk-resume-game-dialog.c		\
	gtk-sgf-tree-signal-proxy.c		\
	gtk-sgf-tree-view.c			\
	gtk-tile-set.c				\
	gtk-utils.c				\
	gui-back-end.c				\
	quarry-history-text-buffer.c		\
	quarry-message-dialog.c			\
	quarry-move-number-dialog.c		\
	quarry-save-confirmation-dialog.c	\
	quarry-stock.c				\
	quarry-text-buffer.c			\
	quarry-text-view.c			\
						\
	gtk-add-or-edit-label-dialog.h		\
	gtk-assistant.h				\
	gtk-clock.h				\
	gtk-color-button.h			\
	gtk-control-center.h			\
	gtk-file-dialog.h			\
	gtk-file-selector.h			\
	gtk-freezable-spin-button.h		\
	gtk-game-info-dialog.h			\
	gtk-games.h				\
	gtk-goban.h				\
	gtk-goban-base.h			\
	gtk-goban-window.h			\
	gtk-go-to-named-node-dialog.h		\
	gtk-gtp-client-interface.h		\
	gtk-help.h				\
	gtk-named-vbox.h			\
	gtk-new-game-dialog.h			\
	gtk-new-game-record-dialog.h		\
	gtk-parser-interface.h			\
	gtk-preferences.h			\
	gtk-progress-dialog.h			\
	gtk-qbox.h				\
	gtk-qhbox.h				\
	gtk-qvbox.h				\
	gtk-resume-game-dialog.h		\
	gtk-sgf-tree-signal-proxy.h		\
	gtk-sgf-tree-view.h			\
	gtk-thread-interface.h			\
	gtk-tile-set.h				\
	gtk-utils.h				\
	gui-back-end.h				\
	quarry-history-text-buffer.h		\
	quarry-message-dialog.h			\
	quarry-move-number-dialog.h		\
	quarry-save-confirmation-dialog.h	\
	quarry-stock.h				\
	quarry-text-buffer.h			\
	quarry-text-view.h			\
						\
	$(LIST_FILES)

nodist_libguigtk_a_SOURCES =	\
	$(LIST_GENERATED_FILES) \
				\
	quarry-marshal.c	\
				\
	quarry-marshal.h

BUILT_SOURCES =			\
	$(LIST_STAMP_FILES)	\
				\
	quarry-marshal.h


# This seems to be the best way of solving circular references.
libguigtk_a_LIBADD = $(top_builddir)/src/gui-utils/libguiutils.a


EXTRA_DIST = quarry-marshal.glist

DISTCLEANFILES = *~

MOSTLYCLEANFILES =		\
	$(LIST_STAMP_FILES)	\
	$(LIST_GENERATED_FILES)	\
				\
	quarry-marshal.c	\
				\
	quarry-marshal.h
