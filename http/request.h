/* A http request... */
#ifndef _REQUEST_H_
#define _REQUEST_H_

typedef struct _http_request *request_t;

typedef enum {
	HEAD,
	GET,
	POST,
	PUT,
	RQ_UNKNOWN
} RQ_TYPE;

extern char *RQ_STRING[];


typedef enum {
	H_REFERER,
	H_CONNECTION,
	H_USER_AGENT,
	H_HOST,
	H_ACCEPT,
	H_ACCEPT_ENCODING,
	H_ACCEPT_CHARSET,
	H_CONTENT_TYPE,
	H_CONTENT_LENGTH,
	H_OTHER,
	H_MAX
} header_types;

struct _http_request {
	RQ_TYPE rtype;
	char *headers[H_MAX];
	char *prot; /* HTTP/1.0 */
	char *path; /* /guestbook.pl */
	char *body; /* name1=value1&name2=value2, for post */
	/* actually content type of object to be returned... messy: */
	char *content_type;
	char *raw; /* the raw request, as received from client */
	int (*destroy)(void *); /* destructor */
};

request_t new_request(char *, request_t);

#endif
