/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */


#include "unistd.h"
#include "stdio.h"
#include "string.h"
#include "server.h"
#include "connection.h"
#include "handler.h"
#include "http.h"
#include "request.h"
#include "ag.h"

/* the "d" is for default */
static void d_handle_con(handler_t);
static int d_get_handler(handler_t, request_t);
static int d_head_handler(handler_t, request_t);
static int d_post_handler(handler_t, request_t);
static int d_default_handler(handler_t, request_t);
static handler_t h_new_handler();

#define h_size sizeof(struct _http_t)
http_t new_http(http_t h) {

	if (!h) h = ag_malloc(h_size);
	(void) new_server(&h->_svr);
	h->_svr.newhandler = h_new_handler;
	h->get_handler = d_get_handler;
	h->post_handler = d_post_handler;
	h->head_handler = d_head_handler;
	h->default_handler = d_default_handler;
	return h;
}

#define SAY_REPLY(FD,R) if ( (strlen(R) > 0) && (write(FD, (void *)R, strlen(R)) <= 0) ) goto out
/* temp, just to see what is returned */
static int d_get_handler(handler_t hd, request_t rq)
{
	connection_t con = hd->con;
	/*http_t h = (http_t)hd->svr;*/

	SAY_REPLY(con->sk, "content-type: text/html\r\n\r\n");
	SAY_REPLY(con->sk, rq->path);
 out:
	return 1;
}

/* temp, just to see what is returned */
static int d_head_handler(handler_t hd, request_t rq)
{
	connection_t con = hd->con;

	SAY_REPLY(con->sk, "content-type: text/html\r\n\r\n");
	SAY_REPLY(con->sk, rq->path);
 out:
	return 1;
}

/* temp, just to see what is returned */
static int d_post_handler(handler_t hd, request_t rq)
{
	connection_t con = hd->con;

	SAY_REPLY(con->sk, "content-type: text/html\r\n\r\n");
	SAY_REPLY(con->sk, rq->body);

 out:
	return 1;
}

/* temp, just to see what is returned */
static int d_default_handler(handler_t hd, request_t rq)
{
	connection_t con = hd->con;

	SAY_REPLY(con->sk, "content-type: text/html\r\n\r\n");
	SAY_REPLY(con->sk, rq->raw);

 out:
	return 1;

}

static handler_t h_new_handler()
{
	handler_t hd;

	hd = new_handler();
	hd->handle_con = d_handle_con;

	return hd;
}


static void d_handle_con(handler_t hd) {
	connection_t con = hd->con;
	http_t h = (http_t)hd->svr;
	/* should really use realloc... 1024 bytes is not exactly
	   a reasonable limit to impose on incoming request lengths... */
	char raw[1024];
	char buf[256];
	int n;
	request_t rq;

	if (!con) return;

	/* try to build up a request? and only service it when we see enter-enter */
	memset((void *)raw, 0, 256);	
	for (;;) {
		memset((void *)buf, 0, 256);	
		n = read(con->sk, (void *)buf, 255);
		if (n > 0) {
			/*fprintf(stderr, "%s", buf);*/
			strcat(raw, buf);
			if (!strstr(raw, "\r\n\r\n")) continue;
			rq = new_request(raw, NULL);

			switch(rq->rtype) {
			case GET:
				(void)h->get_handler(hd, rq);
				break;
			case POST:
				(void)h->post_handler(hd, rq);
				break;
			case HEAD:
				(void)h->head_handler(hd, rq);
				break;
			case PUT:
			case RQ_UNKNOWN:
				(void)h->default_handler(hd, rq);
				break;
			} /* switch */

			rq->destroy(rq);
			goto out;

		} else if (n == -1) {
			goto out;
		} else { /* n == 0 -> EOF */
			goto out;
		}
	} /* for */

 out:
	fprintf(stderr, "d_handle_con: closing connection\n");
	con->destroy(con);

}
