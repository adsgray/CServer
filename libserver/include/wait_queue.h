/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#ifndef _WAIT_QUEUE_H_
#define _WAIT_QUEUE_H_

typedef struct _wait_queue_t *wait_queue_t;

#include <pthread.h>
#include "connection.h"
#include "handler.h"
#include "server.h"
#include "queue.h"


struct _wait_queue_t {
        pthread_mutex_t m;
	pthread_cond_t w;
	int num; /* number of connections in queue */
	int max; /* max number before refusals */
        int num_refused;
	queue_t cons_queue;
	/* returns true if successful */
	int (*add_con)(wait_queue_t, connection_t);
	void (*wait_on)(wait_queue_t, handler_t);
	int (*destroy)(void *);
	/*connection_t conslist;*/
}; 

wait_queue_t new_wait_queue();

#endif
