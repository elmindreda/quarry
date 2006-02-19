# Process this file with Automake to produce `Makefile.in'.

# FIXME: Internationalize `quarry.desktop' and `quarry.xml'.  Probably
#	 need to use `intltool'.

SUBDIRS =		\
	markup-themes	\
	textures


FILES =			\
	gtkrc

filesdir     = $(datadir)/quarry
files_DATA   = $(FILES)


DESKTOP_FILE = quarry.desktop

desktopdir   = $(datadir)/applications
desktop_DATA = $(DESKTOP_FILE)


if REGISTER_MIME_TYPES


MIME_DESCRIPTION_FILE = quarry.xml

mimedir	     = $(datadir)/mime/packages
mime_DATA    = $(MIME_DESCRIPTION_FILE)


install-data-hook: update-mime-database

uninstall-hook: update-mime-database

update-mime-database:
	(update-mime-database $(datadir)/mime				\
	 || echo "*WARNING* failed to update the MIME database")

.PHONY: update-mime-database


endif


EXTRA_DIST =					\
	$(FILES)				\
	$(DESKTOP_FILE:.desktop=.desktop.in)	\
	$(MIME_DESCRIPTION_FILE)


DISTCLEANFILES = *~
