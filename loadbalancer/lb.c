/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#include "wait_queue.h"
#include "connection.h"
#include "handler.h"
#include "ag.h"
#include "lb.h"

static *lb_forwarder_t lb_forwarders; /* an array of forwarding servers */

/* this connection handler is shared between all forwarding servers.
   It checks which server the connection came from and uses
   the corresponding section of the config file (see below) */
static void lb_handle_con(handler_t hd)
{
	server_t svr = hd->svr;
	connection_t con = hd->con;

	hd->state = HD_ACTIVE;

	switch(svr->port) {
		/* this is where we decide which forwarding to do... */
	}
}

static handler_t lb_new_handler()
{
	handler_t hd;
	
	hd = new_handler();
	hd->handle_con = lb_handle_con; /* override */
	/* use default start */
	return hd;
}

int main(int argc, char **argv)
{

	/* read in configuration file. format:
	   <port1>
	   <hostname1>=<pref1>:<porth1>
	   <hostname2>=<pref2>:<porth2>
	   ...
	   <hostnamen>=<prefn>:<porthn>
	   <port2>
	   ...

	   Connections to port1 on the server will be forwarded
	   to the other machines/ports with the relative frequencies
	   shown.
	   
	   One server is started for each local port being forwarded.
	*/
	   
	lb_forwarders = ag_malloc
	return 0;
}
