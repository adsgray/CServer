#
# Copyright (c) 2000 Andrew Gray 
# <agray@alumni.uwaterloo.ca>
#
# See COPYING for details.
#
#

CC=gcc

LIBSERVEROBJS = libserver/ag.o libserver/connection.o \
       libserver/server.o libserver/wait_queue.o \
       libserver/handler.o libserver/queue.o

SMTPOBJS = smtp/smtp_server.o smtp/main.o libserver/libserver.a

CFLAGS += -g -Wall -Ilibserver/include
#LDFLAGS += 

all: libserver.a smtpsink

libserver.a: $(LIBSERVEROBJS)
	ar -r libserver/libserver.a $(LIBSERVEROBJS)

smtpsink: $(SMTPOBJS)
	$(CC) -o smtpsink $(SMTPOBJS) -lpthread

.PHONY: clean

clean:
	-rm $(SMTPOBJS) $(LIBSERVEROBJS) smtpsink
