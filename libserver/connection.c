/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include "connection.h"
#include "ag.h"

#define con_size sizeof(struct _connection_t)

static int destroy_connection(void *arg);

connection_t new_connection()
{
	connection_t con;
	con = ag_malloc(con_size);
	con->destroy = destroy_connection;
	return con;
}

static int destroy_connection(void *arg)
{
	connection_t con = arg;
	if (!con) return 1;

	if (con->clientdata) free(con->clientdata);
	if (con->name) free(con->name);
	if (con->sk > 0) close(con->sk);
	free(con);
	return 0;
}
