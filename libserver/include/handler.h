/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#ifndef _HANDLER_H_
#define _HANDLER_H_

typedef struct _handler_t *handler_t;

#include "wait_queue.h"
#include "handler.h"
#include "connection.h"
#include "server.h"
#include "queue.h"

typedef enum {
	HD_STARTING = 0,
	HD_ACTIVE,
	HD_WAITING,
	HD_STOPPING,
	HD_first_avail /* "subclasses" of handler can start here */
} handler_state_t;

/* in handler.c */
extern char *handler_states[HD_first_avail];

struct _handler_t {
	queue_connector_t _qcon; /* TODO: maybe have server.c keep handlers in a queue? */
        server_t svr;
	int hd_state;
	connection_t con; /* originally handle_con took a connection_t as an arg. It is here
			     now so that on thread cancel it can be destroyed cleanly */
	/* handle_con is responsible for closing the socket */
	void *(*start)(void *);
	void (*handle_con)(handler_t);
	char *(*get_state)(handler_t);
	int (*destroy)(void *); /* "destructor" */
}; 

handler_t new_handler();

#endif
