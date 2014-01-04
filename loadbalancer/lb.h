/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

/* TODO: this whole thing is TODO */

#include "wait_queue.h"
#include "connection.h"
#include "handler.h"
#include "ag.h"

typedef struct _lb_config *lb_config_t;
struct _lb_config {
	/* hostnames and relative frequencies */
}

typedef _lb_stat *lb_stat_t;
struct _lb_stat {
	/* stats for each hostname (somehow) */
}

typedef struct _lb_forwarder *lb_forwarder_t;
struct _lb_forwarder {
	server_t svr;
	lb_config_t conf;
	lb_stat_t stat;
}
