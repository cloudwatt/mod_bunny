NAGIOS_SOURCES  ?= ../nagios-3.5.0
NAGIOS_3_5_X    ?= `echo $(NAGIOS_SOURCES) | grep -Ec "nagios[3]?[-_]3\.5"`

CC      ?= gcc
CFLAGS  ?= -std=gnu99 -W -Wall -g -O2 -shared -fPIC
LDFLAGS ?= -I$(NAGIOS_SOURCES)/include
LDLIBS  ?= -lpthread -lrabbitmq -ljansson

all: mod_bunny.o

mod_bunny.o: mod_bunny.c
	$(CC) $(CFLAGS) $(LDFLAGS) \
		-DNAGIOS_3_5_X=$(NAGIOS_3_5_X) \
		-o mod_bunny.o \
		mb_json.c \
		mb_amqp.c \
		mb_thread.c \
		mod_bunny.c \
		$(LDLIBS)

clean:
	rm -f mod_bunny.o
