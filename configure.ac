AC_PREREQ(2.59)

AC_INIT([office-runner], [0.0], [hadess@hadess.net])
AM_INIT_AUTOMAKE(1.9 dist-bzip2 no-dist-gzip check-news)

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_SED
AM_PROG_CC_C_O

dnl Add the languages which your application supports to po/LINGUAS
GETTEXT_PACKAGE=office-runner
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], [Define to the Gettext package name])
AC_SUBST(GETTEXT_PACKAGE)
IT_PROG_INTLTOOL([0.40.0])

PKG_CHECK_MODULES(OFFICE_RUNNER, gtk+-3.0 gnome-settings-daemon)

AC_CONFIG_FILES([Makefile po/Makefile.in src/Makefile])
AC_OUTPUT
