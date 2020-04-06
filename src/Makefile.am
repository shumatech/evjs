bin_PROGRAMS = evjstest evjscal

if ENABLE_EFFECTS
ENABLE_EFFECTS=1
else
ENABLE_EFFECTS=0
endif

if ENABLE_JOYSTICK
ENABLE_JOYSTICK=1
else
ENABLE_JOYSTICK=0
endif

AM_CFLAGS = -Wall -DENABLE_EFFECTS=$(ENABLE_EFFECTS) -DENABLE_JOYSTICK=$(ENABLE_JOYSTICK)

evjstest_SOURCES = evjstest.c view.c device.c util.c caldb.c barray.c evdev.c jsdev.c \
                   view.h device.h util.h caldb.h barray.h jsdev.h evdev.h
evjstest_CFLAGS = $(ncurses_CFLAGS) $(sqlite3_CFLAGS) $(AM_CFLAGS)
evjstest_LDADD = $(ncurses_LIBS) $(sqlite3_LIBS)

evjscal_SOURCES = evjscal.c util.c caldb.c evdev.c jsdev.c barray.c \
                  util.h caldb.h evdev.h jsdev.h barray.h
evjscal_CFLAGS = $(sqlite3_CFLAGS) $(AM_CFLAGS)
evjscal_LDADD = $(sqlite3_LIBS)
