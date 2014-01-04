/* A http response... */
#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include "request.h"

typedef struct _http_response *response_t;

typedef enum {
	FILE,
	CGI,
	OTHER,
} RES_TYPE;

/* have file response, CGI response subclasses.
   each implement own new_foo_response and implement
     int (*service)(response_t, connection_t con);
   method

   can move spew_file into here...
   say_server_headers too, probably.
 */


/* say headers should be in here... */
struct _http_request {
	RES_TYPE rtype; /* type of request. determines what service does */
	char *headers[H_MAX];
	char *prot; /* HTTP/1.0 */
	char *path; /* /doc/root/guestbook.html */
	char *body; /* name1=value1&name2=value2, for post */
	/* content type of object to be returned: 
	   usually inferred from request?
	*/
	char *content_type;
	char *raw; /* the raw request, as received from client */
	int (*destroy)(void *); /* destructor */
};

response_t new_response(response_t, request_t);

/* cgi will have to do a lot of whacky stuff to
   set up the environment properly for the exec'd
   program:

   - put query string (stuff after file name in path)
     into ENV{QUERY_STRING}. Set other things
     in ENV too (HTTP_REFERER etc...)

   - The body of the request must go into on STDIN
     of the exec'd program

   - Must let the exec'd program set content type.

   - Must get stdoutput of exec'd program and 
     give it to client.
 */
typedef struct _http_cgi_response *http_cgi_response_t;
struct _http_cgi_response {
	struct _http_request parent_class;
	/* other stuff */
}

#endif
