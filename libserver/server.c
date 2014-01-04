/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include "server.h"
#include "handler.h"
#include "connection.h"
#include "wait_queue.h"
#include "ag.h"

/* TODO: handle signals. need a signalh.h possibly, to easily start a
 * signal handling thread, w/ callbacks. handle_signal(int sig) 
 */

/*
notes
generic server code:
1. spawns handler threads
2. accept()s connections and puts these connection
   objects into a queue object that the handlers wait on
3. signals the condition
4. goes back to accept()ing connections
*/

static int setup_connection(server_t);
static connection_t accept_connection(int sock);
static void become_daemon(server_t);
static void become_user(server_t);
static void default_summary(server_t self, int fd);
static int default_destroy_svr(void *);

/* BTW, in case you're wondering, this returns void* so that
 * it can be used as a pthread... 
 */
void *server(void *arg)
{
	server_t svr = arg;
	connection_t con;
	int i, listen_sk;

	if (!svr) return NULL;

	/* 
	 * TODO.
	 * This shouldn't be done here. It doesn't really matter
	 * because it is basically a no-op right now anyway.
	 * A single process can have several servers running 
	 * in it, conceivably.
	 *
	 * Maybe it should be moved to ag.c and left up to main.
	 */
	become_daemon(svr);

	if (!svr->wq) svr->wq = new_wait_queue();
	if (!svr->summary) svr->summary = default_summary;
	if (!svr->destroy) svr->destroy = default_destroy_svr;

	/* configure the wait_queue's max size */
	svr->wq->max = svr->maxcon;

	/* this is the connection handler: */
	svr->handlers = ag_malloc(sizeof(handler_rec) * svr->numthreads);

	for (i = 0; i < svr->numthreads; i++) {
		handler_t hd;
		hd = svr->newhandler();
		hd->svr = svr;
		svr->handlers[i].hd = hd;
		(void) pthread_create(&svr->handlers[i].tid,  /* thread id */
				      NULL,                   /* attributes */
				      hd->start,              /* function to run */
				      (void *)hd              /* arg to pass to function */
			);
	}

	/* 
	 * must do this before changing uids, because we may want to bind
	 * to a port < 1024 and then become a non-root user.
	 *
	 * TODO
	 * At this point we should also open up the logfile.
	*/
	listen_sk = setup_connection(svr);

	/* only change users if svr->uid is non-zero (non-root) */
	if (svr->uid) become_user(svr);

	for (;;) {
		con = accept_connection(listen_sk);
		if (!svr->wq->add_con(svr->wq, con)) con->destroy(con);
	}
}

static int default_destroy_svr(void *arg)
{
	server_t svr = arg;
	/* 
	 * TODO. Some done.
	 * close files 
	 * destroy wait queue
	 * destroy handler threads
	 * remove pid file?
	 */

	if (svr->logfd > 0) (void) close(svr->logfd);
	if (svr->stats) free(svr->stats);
	svr->wq->destroy(svr->wq);
	/* loop and cancel all handler threads. connections may hang.. hmmm. */
	return svr->uid; /* shut up warnings for now */
}

static connection_t accept_connection(int lsock)
{
	int sock;
	struct sockaddr_in *name;
	int len;
	connection_t con;

	name = ag_malloc(sizeof(struct sockaddr_in));
	if ((sock = accept(lsock, name, &len)) <= 0) {
		fprintf(stderr, "accept_connection: error\n");
	}

	if (len > sizeof(struct sockaddr_in)) {
		fprintf(stderr, "accept_connection: memory corruption.\n");
		exit(1);
	}

	con = new_connection();
	con->sk = sock;
	con->name = name;

	return con;
}

static int setup_connection(server_t svr)
{
	int sock;
	struct sockaddr_in name;

	sock = socket(PF_INET, SOCK_STREAM , 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	name.sin_family = AF_INET;
	name.sin_port = htons(svr->port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *) &name, sizeof(name)) < 0)
	{ 
		perror("bind");
		exit(1);
	}

	if (listen(sock, svr->backlog) == -1) {
		perror("listen");
		exit(1);
	}

	return sock;
}

static void become_daemon(server_t svr)
{
	/* TODO: parent writes pidfile after forking, then exit()s 
	   would also be cool to write config to pidfile.
	 */
}

static void become_user(server_t svr)
{
	setuid(svr->uid);
	/* TODO: log this to logfile */
}

static void default_summary(server_t self, int fd)
{
	char buf[512];
	handler_t hd;
	int i;

	memset((void *)buf, 0, 512);	
	sprintf(buf, "%s pidf=%s logf=%s port=%d numth=%d maxcon=%d uid=%d numcon=%d num_ref=%d\n",
		self->name, self->pidfile, self->logfile, self->port, self->numthreads,
		self->maxcon, self->uid, self->wq->num, self->wq->num_refused);
	(void)write(fd, buf, strlen(buf));

	for (i = 0; i < self->numthreads; i++) {
		hd = self->handlers[i].hd;
		memset((void *)buf, 0, 512);	
		sprintf(buf, "handler %d: tid=0x%x state=%s\n",
			i, (int)self->handlers[i].tid, hd->get_state(hd));
		(void)write(fd, buf, strlen(buf));
	}
}
