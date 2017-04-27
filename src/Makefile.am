
bin_PROGRAMS = office-runner

office_runner_SOURCES = office-runner.c
office_runner_LDADD = $(OFFICE_RUNNER_LIBS) -lm
office_runner_CPPFLAGS =				\
	$(OFFICE_RUNNER_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DPKGDATADIR=\""$(pkgdatadir)/"\"

stuff_DATA =				\
	office-runner.ui		\
	jump.wav			\
	pause.wav
stuffdir = $(pkgdatadir)

desktop_in_files = office-runner.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
office-runner.desktop: office-runner.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

appdata_in_files = office-runner.appdata.xml.in
appdatadir = $(datarootdir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
$(appdata_DATA): $(appdata_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(stuff_DATA) $(desktop_in_files) $(appdata_in_files)

CLEANFILES = $(desktop_DATA)

-include $(top_srcdir)/git.mk
