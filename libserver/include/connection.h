/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#ifndef _CONNECTION_H_
#define _CONNECTION_H_

typedef struct _connection_t *connection_t;

#include <sys/socket.h>
#include <netinet/in.h>
#include "queue.h"

struct _connection_t {
	queue_connector_t _qcon;
	int sk; /* socket fd */
	struct sockaddr_in *name;
	void *clientdata; /* extra */
	connection_t prev, next;
	int (*destroy)(void *); /* destructor */
};

connection_t new_connection();

#endif
