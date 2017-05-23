lib_LTLIBRARIES = mod_bunny.la

AM_CPPFLAGS=${NAGIOS_INCLUDE}

mod_bunny_la_SOURCES = \
    mb_amqp.c mb_amqp.h \
    mb_hash.c mb_hash.h \
    mb_json.c mb_json.h \
    mb_thread.c \
    mod_bunny.c mod_bunny.h

mod_bunny_la_LDFLAGS = -module -version-info $(LIBMB_MAJOR):$(LIBMB_MINOR):$(LIBMB_MICRO)
mod_bunny_la_LDLIBS = -lpthread -lrabbitmq -ljansson

ACLOCAL_AMFLAGS = -I m4
