# Process this file with autoconf to produce a configure script.
AC_INIT(Mousepad, 0.2.1+, erikharrison@gmail.com)
AM_INIT_AUTOMAKE($PACKAGE_TARNAME, $PACKAGE_VERSION)
AC_CONFIG_SRCDIR([src/mousepad.h])
AM_CONFIG_HEADER([config.h])


# Checks for programs. Need to check for xfprint here.
AC_PROG_CC
AC_PROG_INSTALL
CFLAGS="$CFLAGS -Wall"

XDT_CHECK_PACKAGE([LIBXFCEGUI4], [libxfcegui4-1.0], [4.2.0])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.2.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-2.0], [2.2.0])

BM_DEBUG_SUPPORT

XDT_I18N([ca cs de es fr hu it ja lt pl ru sk sv ta zh_CN zh_TW])

AC_CONFIG_FILES([Makefile
                 po/Makefile.in
                 src/Makefile])
AC_OUTPUT
