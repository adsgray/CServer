#ifndef _HTTP_H_
#define _HTTP_H_

typedef struct _http_t *http_t;

#include "handler.h"
#include "server.h"
#include "request.h"

/*
 * For both POST and GET:
 * Have array of - file type handlers.
 *               - path handlers
 */
struct _http_t {
	struct _server_t _svr;
	char *docroot; /* path of "/" on server */
	/* return 1 if the request was handled, 0 otherwise */
	int (*get_handler)(handler_t, request_t);
	int (*head_handler)(handler_t, request_t);
	int (*post_handler)(handler_t, request_t);
	int (*default_handler)(handler_t, request_t);
};

http_t new_http(http_t);

#endif
