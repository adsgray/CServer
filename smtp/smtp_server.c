/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

/*
 * This whole file looks like a big hack for a good reason.
 *
 * Hint: This whole file is a big hack.
 * 
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

#define SMTP_INFO_NAME "SMTPinfo v0.9.0\r\n"
#define SMTP_GREETING "220 SMTPsink v0.9.0\r\n"
#define DEFAULT_REPLY "250 looks good\r\n"
#define DATA_REPLY    "354 Go ahead...\r\n"
#define QUIT_REPLY    "220 see ya\r\n"
#define ERR_4XX_REPLY "450 temp error\r\n"
#define ERR_5XX_REPLY "550 permanent error\r\n"
#define HELO          "helo"
#define MAIL_FROM     "mail from:"
#define RCPT_TO       "rcpt to:"
#define DATA          "data\r\n"
#define DOT           ".\r\n"
#define DOT2          "\r\n.\r\n"
#define SAY_REPLY(R) if ( (strlen(R) > 0) && (write(con->sk, (void *)R, strlen(R)) <= 0) ) goto out

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
static int predicate(int percent);
static int SMTP_set_command(char *cmd);

static handler_t SMTP_new_handler()
{
	handler_t hd;
	
	hd = new_handler();
	hd->handle_con = SMTP_with_options;
	/* use default start */
	return hd;
}


static handler_t SMTP_Info_handler()
{
	handler_t hd;
	
	hd = new_handler();
	hd->handle_con = SMTP_info_con;
	/* use default start */
	return hd;
}

static server_t SMTP_server;
static server_t SMTP_info_server;

/* you can start this entire thing as a thread itself */
void *smtp_server(void *arg)
{
        pthread_t tid;

	SMTP_server = new_server(NULL);
	SMTP_server->name = SMTP_GREETING;
	SMTP_server->pidfile = "/tmp/smtpsink.pid";
	SMTP_server->logfile = "/tmp/smtpsink.log";
	SMTP_server->port = 2525;
	SMTP_server->backlog = 15;
	SMTP_server->numthreads = 35;
	SMTP_server->maxcon = 50;
	SMTP_server->newhandler = SMTP_new_handler;
#ifdef SMTP_STATS
	SMTP_server->stats = ag_malloc(sizeof(int) * SMTP_stat_end);
#endif

	SMTP_info_server = new_server(NULL);
	SMTP_info_server->name = SMTP_INFO_NAME;
	SMTP_info_server->port = 2552;
	SMTP_info_server->backlog = 2;
	SMTP_info_server->numthreads = 5;
	SMTP_info_server->maxcon = 5;
	SMTP_info_server->newhandler = SMTP_Info_handler;

	/* could pass in a struct with params telling
	   max random delays, frequency of 4xx/5xx replies, etc */
        options = arg;

	/* start the info server */
	(void) pthread_create(&tid, NULL, SMTP_info_server->start, (void *)SMTP_info_server);

	/* never returns */
	(void) SMTP_server->start((void *)SMTP_server);

	return NULL;
}

/*
 * TODO:
 * Note that there are no locks around any of the options->foo assignments.
 * So different people connected to the info server can race to assign
 * to these variables.
 *
 * Boo hoo.
 *
 * ...
 * TODO: make these options generalized...
 * struct int_option {
 *       char *name
 *       int  *target;
 * };
 *
 * Have an array of these buggers, and if options[i]->name matches
 * cmd, set *options[i]->target to num.
 * Could of course have a separate loop for string options.
 *
 * ...
 * TODO: make the above suggestion into a part of libserver...
 *
 */
static int SMTP_set_command(char *cmd)
{
	int num;
	char *pos;

	if (!strlen(cmd)) return 0;

	if ((pos = strstr(cmd, "min_delay")) != NULL) {
		pos += strlen("min_delay") + 1;
		num = atoi(pos);
		options->min_delay = num;
	}
	else if ((pos = strstr(cmd, "max_delay")) != NULL) {
		pos += strlen("max_delay") + 1;
		num = atoi(pos);
		options->max_delay = num;
	}
	else if ((pos = strstr(cmd, "percent_4xx")) != NULL) {
		pos += strlen("percent_4xx") + 1;
		num = atoi(pos);
		options->percent_4xx = num;
	}
	else if ((pos = strstr(cmd, "percent_5xx")) != NULL) {
		pos += strlen("percent_5xx") + 1;
		num = atoi(pos);
		options->percent_5xx = num;
	}
	else if ((pos = strstr(cmd, "percent_dropped")) != NULL) {
		pos += strlen("percent_dropped") + 1;
		num = atoi(pos);
		options->percent_dropped = num;
	} else return 0;

	return 1;
}

static void SMTP_info_con(handler_t self)
{
	char buf[256];
	size_t n;
	server_t svr = self->svr;
	connection_t con = self->con;
	char *info_prompt = "> ";

	if (!con) return;

	self->hd_state = HD_ACTIVE;

	fprintf(stderr, "SMTP_info_con\n");
	(void)write(con->sk, svr->name, strlen(svr->name));

	do {
		SAY_REPLY(info_prompt);
                memset((void *)buf, 0, 256);	
		n = read(con->sk, (void *)buf, 255);
		if (n > 0) {
			/*if (write(con->sk, (void *)buf, strlen(buf)) == -1)
			  goto out;*/
			if (strncmp(buf, "info", 4) == 0)
				SMTP_summary(svr, con->sk);
			else if (strncmp(buf, "kill", 4) == 0) {
				/* kill this entire process... how? */;
				SMTP_server->destroy(SMTP_server);
				SAY_REPLY("SMTP_server destroyed... now what?\r\n");
			}
			else if (strncmp(buf, "help", 4) == 0) {
				SAY_REPLY("Commands:\r\n");
				SAY_REPLY("info       -- get info on threads.\r\n");
				SAY_REPLY("kill       -- kill server. Currently unclean.\r\n");
				SAY_REPLY("set opt n  -- set a server variable.\r\n");
				SAY_REPLY("              recognized opts are:\r\n");
				SAY_REPLY("       min_delay: minimum delay for responses\r\n");
				SAY_REPLY("       max_delay: maximum delay for responses\r\n");
				SAY_REPLY("     percent_4xx: percentage of 4xx responses\r\n");
				SAY_REPLY("     percent_5xx: percentage of 5xx responses\r\n");
				SAY_REPLY(" percent_dropped: probability of dropping con NOW\r\n");
				SAY_REPLY("\r\n");
			}
			else if (strncmp(buf, "set ", 4) == 0) {
				if (!SMTP_set_command(buf + 4)) 
					SAY_REPLY("set command error\r\n");
			} else {
				SAY_REPLY("type \"help\" for help\r\n");
			}
		} else if (n == -1) {
			fprintf(stderr, "SMTP_info_con: closing connection\n");
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
		SMTP_server->name, SMTP_server->pidfile, SMTP_server->logfile, 
		SMTP_server->port, SMTP_server->numthreads,
		SMTP_server->maxcon, SMTP_server->uid, 
		SMTP_server->wq->num, SMTP_server->wq->num_refused);
	(void)write(fd, buf, strlen(buf));

	server_handler_summary(SMTP_server, fd);


#ifdef SMTP_STATS
	memset((void *)buf, 0, 512);	
	sprintf(buf, "%s cons=%d bytes=%d 4xx=%d 5xx=%d dropped=%d\n\n",
		SMTP_server->name, 
		SMTP_server->stats[SMTP_connections_processed],
		SMTP_server->stats[SMTP_bytes_processed],
		SMTP_server->stats[SMTP_4xx],
		SMTP_server->stats[SMTP_5xx],
		SMTP_server->stats[SMTP_dropped]);
	(void)write(fd, buf, strlen(buf));
#endif

	memset((void *)buf, 0, 512);	
	sprintf(buf, "min_delay=%d max_delay=%d percent_4xx=%d percent_5xx=%d percent_dropped=%d\n\n",
		options->min_delay, options->max_delay, options->percent_4xx,
		options->percent_5xx, options->percent_dropped);
	(void)write(fd, buf, strlen(buf));

}

static int predicate(int percent)
{
	int foo;
	foo = ((rand() >> 5) % 100) + 1;
	return (foo <= percent);
}

/* return a random error message, taking the state into account 
 * ugly: uses global "options".
 * This function also handles some SMTP state transitions.
 */
static char *get_random_reply(server_t svr,  /* for stats */
			      SMTP_cstate *st)
{
	/*
	 * return a state in st. ugly. foo.
	 */
	char *rval = DEFAULT_REPLY;
	int p4xx, p5xx;

	p4xx = predicate(options->percent_4xx);
	p5xx = predicate(options->percent_5xx);

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
		if (p5xx) {
			SMTP_stat(svr->stats[SMTP_5xx], 1) 
			rval = ERR_5XX_REPLY;
			*st = ST_MAIL_FROM;
		} else {
			*st = ST_RCPT_TO;
		}
		break;
	case ST_RCPT_TO:
		/* state stays the same -- multiple rcpt's */
		if (p4xx) {
			SMTP_stat(svr->stats[SMTP_4xx], 1) 
			rval = ERR_4XX_REPLY;
		}
		else if (p5xx) {
			SMTP_stat(svr->stats[SMTP_5xx], 1) 
			rval = ERR_5XX_REPLY;
		} 
		break;
	}

	return rval;
}


/* randomly delay based on state */
static void do_random_delay(SMTP_cstate st)
{
	int delay_window = options->max_delay - options->min_delay + 1;

	sleep(((rand() >> 5) % delay_window) + options->min_delay);
	st = ST_HELO; /* shut up warnings */
}

static int random_close() 
{
	return predicate(options->percent_dropped);
}


/* doesn't do much right now.
 * Not a completely correct simulation... it will accept DATA without
 * RCPT TO, I think.
 */
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
		if (random_close()) {
			SMTP_stat(svr->stats[SMTP_dropped], 1) 
			goto out;
		}
                memset((void *)buf, 0, 256);	
		n = read(con->sk, (void *)buf, 255);
		do_random_delay(st);

		/* a simple SMTP FSM 
		 * problem: state transitions must depend on errors...
		 */
		if (n > 0) {
			SMTP_stat(svr->stats[SMTP_bytes_processed], n) 
			if (st != ST_DOT && !strncasecmp(buf, "quit", 4)) {
				SAY_REPLY(QUIT_REPLY);
				break; 
			}
			reply = DEFAULT_REPLY;
			/*fprintf(stderr, "buf=%s|", buf);*/
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
					/*fprintf(stderr, "DOT\n");*/
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

