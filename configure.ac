AC_PREREQ(2.59)

AC_INIT([office-runner], [1.0.2], [hadess@hadess.net])
AM_INIT_AUTOMAKE(1.9 dist-xz no-dist-gzip check-news)

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_SED
AM_PROG_CC_C_O

dnl Add the languages which your application supports to po/LINGUAS
GETTEXT_PACKAGE=office-runner
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], [Define to the Gettext package name])
AC_SUBST(GETTEXT_PACKAGE)
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

PKG_CHECK_MODULES(OFFICE_RUNNER, gtk+-3.0 >= 3.8 upower-glib >= 0.99.0)

AC_CONFIG_FILES([
	Makefile
	po/Makefile.in
	icons/Makefile
	icons/16x16/Makefile
	icons/22x22/Makefile
	icons/24x24/Makefile
	icons/32x32/Makefile
	icons/48x48/Makefile
	icons/256x256/Makefile
	src/Makefile])
AC_OUTPUT
