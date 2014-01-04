/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

/*
 * This whole file looks like a big hack for a good reason.
 */

#include <pthread.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "server.h"
#include "handler.h"
#include "ag.h"
#include "smtp_server.h"

#define SMTP_STATS

#define SMTP_GREETING "220 SMTPsink v0.1\r\n"
#define DEFAULT_REPLY "250 looks good\r\n"
#define DATA_REPLY    "354 Go ahead...\r\n"
#define ERR_4XX_REPLY "450 temp error\r\n"
#define ERR_5XX_REPLY "550 permanent error\r\n"
#define HELO          "helo"
#define MAIL_FROM     "mail from:"
#define RCPT_TO       "rcpt to:"
#define DATA          "data\r\n"
#define DOT           ".\r\n"
#define DOT2          "\r\n.\r\n"
#define SAY_REPLY(R) if ( (strlen(R) > 0) && (write(con->sk, (void *)R, strlen(R)) <= 0) ) goto out

#ifdef SMTP_STATS
/* crap. the threads will race to update these. Need a mutex */
typedef enum {
	SMTP_connections_processed = 0,
	SMTP_bytes_processed,
	SMTP_4xx,
	SMTP_5xx,
	SMTP_dropped,
	SMTP_stat_end
} SMTP_stat_type;

/* SMTP protocol states. The state defines what we are expecting
 * the client to send us.
 */
typedef enum {
	ST_HELO = HD_first_avail,
	ST_MAIL_FROM,
	ST_RCPT_TO,
	ST_DATA,
	ST_DOT,
	ST_QUIT,
} SMTP_cstate;

pthread_mutex_t stat_mutex = PTHREAD_MUTEX_INITIALIZER;

/* call like SMTP_stat(svr->stats[SMTP_4xx], 1) 
             SMTP_stat(svr->stats[SMTP_bytes_processed], 243) */
#define SMTP_stat(which,by_what) pthread_mutex_lock(&stat_mutex); which += by_what; \
                                 pthread_mutex_unlock(&stat_mutex);
#else 
#define SMTP_stat(which,by_what) 
#endif /* SMTP_STATS */


static smtp_server_options_t options;
static void SMTP_info_con(handler_t self);
static void SMTP_with_options(handler_t self);
static void SMTP_summary(server_t self, int fd);
static void do_random_delay(SMTP_cstate st);
static int random_close();
static char *get_random_reply(server_t svr, SMTP_cstate *st);

static handler_t SMTP_new_handler()
{
	handler_t hd;
	
	hd = new_handler();
	hd->handle_con = SMTP_with_options;
	/* use default start */
	return hd;
}

static struct _server_t SMTP_server = {
	SMTP_GREETING,       /* name */
	"/tmp/smtpsink.pid", /* pidfile */
	"/tmp/smtpsink.log", /* logfile */
	0,                  /* logfd */
	NULL,               /* stats */
        2525,		/* port */
	15,		/* backlog see listen(2) */
	35,		/* numthreads */
	50,		/* max unhandled connections */
	0,		/* uid. stay as root */
	NULL,           /* handlers */
	SMTP_new_handler, /* get a new smtp connection handler */
	NULL,             /* summary. use default */
	NULL,             /* destructor. use default */
	NULL             /* wait queue. will be filled in for us */
};


static handler_t SMTP_Info_handler()
{
	handler_t hd;
	
	hd = new_handler();
	hd->handle_con = SMTP_info_con;
	/* use default start */
	return hd;
}

static struct _server_t SMTP_Info_Server = {
	"SMTPsink Info Server v0.1\r\n",
	"/tmp/smtpsink.info.pid", /* pidfile */
	NULL,                     /* logfile */
	0,                        /* logfd */
	NULL,                     /* stats */
	2552,                     /* port */
	2,                        /* backlog */
	2,                        /* threads */
	5,                        /* max unhandled */
	0,                        /* uid */
	NULL,                     /* handlers */
	SMTP_Info_handler,        /* get a new handler */
	NULL,                     /* summary */
	NULL,             /* destructor. use default */
	NULL                      /* wait queue */
};


/* you can start this entire thing as a thread itself */
void *smtp_server(void *arg)
{
        pthread_t tid;
	/* could pass in a struct with params telling
	   max random delays, frequency of 4xx/5xx replies, etc */
        options = arg;

#ifdef SMTP_STATS
	SMTP_server.stats = ag_malloc(sizeof(int) * SMTP_stat_end);
#endif

	/* start the info server */
	(void) pthread_create(&tid, NULL, server, (void *)&SMTP_Info_Server);

	/* never returns */
	(void) server((void *)&SMTP_server);

	return NULL;
}


static void SMTP_info_con(handler_t self)
{
	char buf[256];
	size_t n;
	server_t svr = self->svr;
	connection_t con = self->con;

	if (!con) return;

	self->hd_state = HD_ACTIVE;

	fprintf(stderr, "SMTP_info_con\n");
	(void)write(con->sk, svr->name, strlen(svr->name));

	do {
                memset((void *)buf, 0, 256);	
		n = read(con->sk, (void *)buf, 255);
		if (n > 0) {
			/*if (write(con->sk, (void *)buf, strlen(buf)) == -1)
			  goto out;*/
			if (strncmp(buf, "info", 4) == 0)
				SMTP_summary(svr, con->sk);
			else if (strncmp(buf, "kill", 4) == 0)
				/* kill this entire process... how? */;
		} else if (n == -1) {
			fprintf(stderr, "closing connection\n");
			goto out;
		} else { /* n == 0 -> EOF */
			goto out;
		}
	} while (strncmp(buf, "quit", 4) != 0);

 out:
	/* ya, I know that both servers share the stat mutex even though
	   they are updating separate stat arrays. I don't care */
#if 0
	/* oops... no stats enabled for the info server. That explains the SEGV. */
	SMTP_stat(svr->stats[SMTP_connections_processed], 1) 
#endif
	con->destroy(con);
}

static void server_handler_summary(server_t svr, int fd)
{
	char buf[512];
	int i;
	handler_t hd;

	for (i = 0; i < svr->numthreads; i++) {
		hd = svr->handlers[i].hd;
		memset((void *)buf, 0, 512);	
		sprintf(buf, "handler %d: tid=0x%x state=%s\n",
			i, (int)svr->handlers[i].tid, hd->get_state(hd));
		(void)write(fd, buf, strlen(buf));
	}
}

static void SMTP_summary(server_t self, int fd)
{
	char buf[512];

	if (!self) return;

	memset((void *)buf, 0, 512);	
	sprintf(buf, "%s pidf=%s port=%d numth=%d maxcon=%d\n uid=%d numcon=%d num_ref=%d\n\n",
		self->name, self->pidfile, self->port, self->numthreads,
		self->maxcon, self->uid, self->wq->num, self->wq->num_refused);
	(void)write(fd, buf, strlen(buf));

	server_handler_summary(self, fd);

	memset((void *)buf, 0, 512);	
	sprintf(buf, "%s pidf=%s logf=%s port=%d numth=%d\n maxcon=%d uid=%d numcon=%d num_ref=%d\n\n",
		SMTP_server.name, SMTP_server.pidfile, SMTP_server.logfile, 
		SMTP_server.port, SMTP_server.numthreads,
		SMTP_server.maxcon, SMTP_server.uid, 
		SMTP_server.wq->num, SMTP_server.wq->num_refused);
	(void)write(fd, buf, strlen(buf));

	server_handler_summary(&SMTP_server, fd);


#ifdef SMTP_STATS
	memset((void *)buf, 0, 512);	
	sprintf(buf, "%s cons=%d bytes=%d 4xx=%d 5xx=%d dropped=%d\n\n",
		SMTP_server.name, 
		SMTP_server.stats[SMTP_connections_processed],
		SMTP_server.stats[SMTP_bytes_processed],
		SMTP_server.stats[SMTP_4xx],
		SMTP_server.stats[SMTP_5xx],
		SMTP_server.stats[SMTP_dropped]);
	(void)write(fd, buf, strlen(buf));
#endif

}


/* return a random error message, taking the state into account 
 * ugly: use global "options".
 * This function also handles some SMTP state transitions.
 */
static char *get_random_reply(server_t svr,  /* for stats */
			      SMTP_cstate *st)
{
	/* TODO: state transitions depend on reply codes... how to do? 
	 * return a state in st. ugly. foo.
	 */
	char *rval = DEFAULT_REPLY;

	switch(*st) {
	case ST_HELO:
	case ST_DATA:
		/* can't really have errors here */
		break;
	case ST_DOT:
		*st = ST_MAIL_FROM;
		break;
	case ST_MAIL_FROM:
		/* if we are returning a 4xx/5xx error, keep state
		   in ST_MAIL_FROM */
		*st = ST_RCPT_TO;
		break;
	case ST_RCPT_TO:
		/* state stays the same -- multiple rcpt's */
		break;
	}

	return rval;
}


/* randomly delay based on state */
static void do_random_delay(SMTP_cstate st)
{
	st = ST_HELO; /* shut up warnings */
	sleep(2);
}

static int random_close() 
{
	/* decide whether we should drop the connection or not */
	return 0;
}


/* doesn't do much right now */
static void SMTP_with_options(handler_t self) 
{
	char buf[256];
	size_t n;
	server_t svr = self->svr;
	connection_t con = self->con;
	SMTP_cstate st = ST_HELO;
	char *reply = NULL;

	self->hd_state = HD_ACTIVE;

	if (!con) return;

	fprintf(stderr, "SMTP_with_options\n");
	do_random_delay(st);
	SAY_REPLY(SMTP_GREETING);

	for (;;) { /* loop has breaks */
		if (random_close()) goto out;
                memset((void *)buf, 0, 256);	
		n = read(con->sk, (void *)buf, 255);
		do_random_delay(st);

		/* a simple SMTP FSM 
		 * TODO: add delays, random errors, etc.
		 * problem: state transitions must depend on errors...
		 */
		if (n > 0) {
			SMTP_stat(svr->stats[SMTP_bytes_processed], n) 
			if (st != ST_DOT && !strncasecmp(buf, "quit", 4)) break; 
			reply = DEFAULT_REPLY;
			fprintf(stderr, "buf=%s|", buf);
			switch(st) {
			case ST_HELO:
				/* demand a helo */
				if (strncasecmp(buf, HELO, strlen(HELO)) != 0) {
					SMTP_stat(svr->stats[SMTP_5xx], 1) 
					reply = ERR_5XX_REPLY;
				} else {
					st = ST_MAIL_FROM;
				}
				break;
			case ST_MAIL_FROM:
				if (strncasecmp(buf, MAIL_FROM, strlen(MAIL_FROM)) != 0) {
					SMTP_stat(svr->stats[SMTP_5xx], 1) 
					reply = ERR_5XX_REPLY;
				} else {
					reply = get_random_reply(svr, &st);
				}
				break;
			case ST_RCPT_TO:
			case ST_DATA:
				if (!strncasecmp(buf, RCPT_TO, strlen(RCPT_TO))) {
					/* can have multiple RCPT_TO's */
					reply = get_random_reply(svr, &st);
				} else if (!strncasecmp(buf, DATA, strlen(DATA))) {
					st = ST_DOT;
					reply = DATA_REPLY;
				} else {
					SMTP_stat(svr->stats[SMTP_5xx], 1) 
					reply = ERR_5XX_REPLY;
				}
				break;
			case ST_DOT:
			        /* this means we are in data stage... so don't
				   say anything until there is a line with
				   a dot on a line by itself */
				if (!strncasecmp(buf, DOT, strlen(DOT))
				    || (strstr(buf, DOT2) != NULL )) {
					fprintf(stderr, "DOT\n");
					/* allow multiple emails */
					reply = get_random_reply(svr, &st);
				} else reply = "";
				break;
			} /* switch */

			SAY_REPLY(reply);

		} else if (n == -1) {
			fprintf(stderr, "SMTP_with_options: closing connection\n");
			goto out;
		} else { /* n == 0 -> EOF */
			goto out;
		}

	} 

  
 out:
	SMTP_stat(svr->stats[SMTP_connections_processed], 1) 
	con->destroy(con);
  
}

