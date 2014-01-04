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
       libserver/handler.o libserver/queue.o libserver/gr.o

SMTPOBJS = smtp/smtp_server.o smtp/main.o libserver/libserver.a
HTTPOBJS = http/http.o http/http_driver.o http/request.o libserver/libserver.a

CFLAGS += -g -Wall -Ilibserver/include
#LDFLAGS += 

all: libserver.a smtpsink http_driver

libserver.a: $(LIBSERVEROBJS)
	ar -r libserver/libserver.a $(LIBSERVEROBJS)

smtpsink: $(SMTPOBJS)
	$(CC) -o smtpsink $(SMTPOBJS) -lpthread

http_driver: $(HTTPOBJS)
	$(CC) -o http_driver $(HTTPOBJS) -lpthread

.PHONY: clean

clean:
	-rm $(SMTPOBJS) $(HTTPOBJS) $(LIBSERVEROBJS) smtpsink http_driver
