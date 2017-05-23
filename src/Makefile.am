lib_LTLIBRARIES = libmod_bunny.la

AM_CPPFLAGS=${NAGIOS_INCLUDE}

libmod_bunny_la_SOURCES = \
    mb_amqp.c mb_amqp.h \
    mb_hash.c mb_hash.h \
    mb_json.c mb_json.h \
    mb_thread.c \
    mod_bunny.c mod_bunny.h

libmod_bunny_la_LDFLAGS = -version-info $(LIBMB_MAJOR):$(LIBMB_MINOR):$(LIBMB_MICRO)
libmod_bunny_la_LDLIBS = -lpthread -lrabbitmq -ljansson

ACLOCAL_AMFLAGS = -I m4
