/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#ifndef _SERVER_H_
#define _SERVER_H_

typedef struct _server_t *server_t;

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "handler.h"
#include "wait_queue.h"

typedef struct {
	pthread_t tid;
	handler_t hd;
} handler_rec;

struct _server_t {
        char *name;
        char *pidfile;
        char *logfile;
        int logfd;
        int *stats; /* place holder for derived classes to track various things */
	uint16_t port;
	int backlog; /* see listen(2) */
	int numthreads;
	int maxcon; /* max number of connections before refusal */
	int uid; /* user to become after binding to port */
        handler_rec *handlers; /* array of child thread ids */
	handler_t (*newhandler)(void); /* get a new handler[1] */
        void (*summary)(server_t, int); /* print stats summary */
	int (*destroy)(void *); /* "destructor" */
        wait_queue_t wq;
}; 

/*
 * [1] You must override this. It must return a handler_t that has
 * a valid handle_con method.
 */

/* pass it a server_t */
void *server(void *arg);


#endif
