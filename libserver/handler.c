/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include "handler.h"
#include "connection.h"
#include "ag.h"

#define hd_size sizeof(struct _handler_t)

char *handler_states[HD_first_avail] = {
		"HD_STARTING",
		"HD_ACTIVE",
		"HD_WAITING",
		"HD_STOPPING"
	};

static char *default_get_state(handler_t self)
{
	return handler_states[self->hd_state];
}

static int default_destroy_hd(void *arg)
{
	handler_t hd = arg;
	if (!hd) return 1;

	hd->hd_state = HD_STOPPING;
	hd->con->destroy(hd->con);

	/* TODO: maybe log something to the server's logfile? */
	free(hd);
	hd = NULL;

	return 0;
}

static void *default_start(void *arg)
{
	handler_t self = arg;
	wait_queue_t wq = self->svr->wq;

	if (!self) pthread_exit(NULL);

	self->hd_state = HD_STARTING;
	
	wq->wait_on(wq, self);

	return NULL; /* shut up warnings */
}

static void default_handle_con(handler_t self)
{
	server_t svr = self->svr;
	connection_t con = self->con;

	if (!con) return;

	self->hd_state = HD_ACTIVE;

	/* presumably do something with connection */
	if (svr->name) {
	  write(con->sk, svr->name, strlen(svr->name));
	  write(con->sk, "\r\n", 2);
	}

	con->destroy(con);
}

handler_t new_handler()
{
	handler_t hd;

	hd = ag_malloc(hd_size);
	hd->start = default_start;
	hd->handle_con = default_handle_con;
	hd->get_state = default_get_state;
	hd->destroy = default_destroy_hd;
	return hd;
}
