AC_PREREQ(2.59)

AC_INIT([evjs], [0.1], [scott@shumatech.com])

AM_INIT_AUTOMAKE([1.9 foreign])

AC_CONFIG_HEADERS([config.h])
AC_PROG_CC

PKG_CHECK_MODULES([ncurses], [ncurses])
PKG_CHECK_MODULES([sqlite3], [sqlite3])

AC_ARG_ENABLE([effects],
    AS_HELP_STRING([--disable-effects], [Disable force feedback effects support]))

AM_CONDITIONAL([ENABLE_EFFECTS], [test "x$enable_effects" != "xno"])

AC_ARG_ENABLE([joystick],
    AS_HELP_STRING([--disable-joystick], [Disable joystick device support]))

AM_CONDITIONAL([ENABLE_JOYSTICK], [test "x$enable_joystick" != "xno"])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
