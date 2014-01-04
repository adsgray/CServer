
#include "request.h"
#include "ag.h"
#include <string.h>
#include <ctype.h>

static RQ_TYPE get_rq_type(char *raw);
static char *get_path(char *raw);
static char *get_prot(char *raw);
int destroy_request(void *arg);
static char *str_toupper(char *fr);
static char *get_content_type(char *path);
static void get_headers(request_t rq, char *raw);

request_t new_request(char *raw, request_t rq);
int destroy_request(void *arg);

#define rq_size sizeof(struct _http_request)

#define STR_GET "GET"
#define STR_POST "POST"
#define STR_HEAD "HEAD"
#define STR_PUT "PUT" /* not really used? */

char *RQ_STRING[] = { "HEAD", "GET", "POST", "PUT", "RQ_UNKNOWN"};

static RQ_TYPE get_rq_type(char *raw)
{
	/* easy to add new request types... */
	if (!strncasecmp(raw, STR_GET, strlen(STR_GET))) return GET;
	if (!strncasecmp(raw, STR_POST, strlen(STR_POST))) return POST;
	if (!strncasecmp(raw, STR_HEAD, strlen(STR_HEAD))) return HEAD;
	if (!strncasecmp(raw, STR_PUT, strlen(STR_PUT))) return PUT;
	return RQ_UNKNOWN;
}

/* instead of doing these mallocs, could just index into raw... */

/* returns path segment of raw request */
static char *get_path(char *raw)
{
	char *start_path;
	char *end_path;
	char *rval;
	int plength;

	start_path = strstr(raw, " ");;
	if (!start_path) return NULL;
	start_path++;
	end_path = strstr(start_path, " ");
	if (!end_path) return NULL;
	end_path--;
	plength = end_path - start_path + 1;

	rval = ag_malloc(plength + 1); /* calloc()d */

	strncpy(rval, start_path, plength);
	return rval;
}

/* returns protocol segment of raw request */
static char *get_prot(char *raw)
{
	char *start_prot;
	char *end_prot;
	char *rval;
	int plength;
	
	start_prot = strstr(raw, " ");
	if (!start_prot) return NULL;
	start_prot++;
	start_prot = strstr(start_prot, " ");
	if (!start_prot) return NULL;
	start_prot++;
	end_prot = strstr(start_prot, "\r\n");
	if (!end_prot) return NULL;
	end_prot--;
	plength = end_prot - start_prot + 1;

	rval = ag_malloc(plength + 1); /* calloc()d */
	strncpy(rval, start_prot, plength);
	return rval;
}


/* raw is the raw request, ie:

What comes in for a post: 
POST /cgi-bin/guestbooks.pl HTTP/1.0
Referer: http://localhost:8008/guestbook.html
Connection: Keep-Alive
User-Agent: Mozilla/4.51 [en] (X11; I; Linux 2.2.17pre20 i586)
Host: localhost:8008
Accept: image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, image/png
Accept-Encoding: gzip
Accept-Language: en
Accept-Charset: iso-8859-1,*,utf-8
Content-type: application/x-www-form-urlencoded
Content-length: 121

act=r&name=Andrew+Gray&email=agray%40undergrad.math.uwaterloo.ca&URL=http%3A%2F%2Ffoo.bar.com&comments=this+website+sucks
*/

/* must match up with header_types */
static char *header_names[] = { "Referer:",
                                "Connection:",
				"User-Agent:",
				"Host:",
				"Accept:",
				"Accept-Encoding:",
				"Accept-Charset:",
				"Content-type:",
				"Content-length:",
				"other"};
/* 
  incomplete: 
  want to fill in rq->headers for all of the standard headers
  defined in request.h.
  handlers can then search the raw request itself
  for other headers they are interested in.

  maybe "\r\n" and "\r\n\r\n" should be constants?
*/
static void get_headers(request_t rq, char *raw) {

	int i;
	char *pos = raw;
	char *end_headers;

	end_headers = strstr(raw, "\r\n\r\n");
	
	/* skip \r\n */
	pos += 2;
	

	/* assume that the first line of raw is the GET/POST line: */

	for (;;) {
		pos = strstr(pos, "\r\n"); /* skip to next header */
		if (!pos || pos == end_headers) return;
		pos += 2;
		for (i = 0; i < H_MAX; i++) {
			if (!strncmp(pos,
				header_names[i],
				strlen(header_names[i])))
			{
				/* matched on header i.
				   should check return values of strstr
				   here...
				*/
				char *tmp1, *tmp2;
				/* end of header name */
				tmp1 = pos + strlen(header_names[i]);
				/* end of header contents */
				tmp2 = strstr(tmp1,"\r\n");
				/* temp null terminate */
				*tmp2 = '\0';
				rq->headers[i] = strdup(tmp1);
				*tmp2 = '\r'; /* restore */
				break;
			} 
		}
	}
}

/* content types.
   zip, gz not here yet.
 */
static char* CT_TEXTHTML = "text/html";
static char* CT_TEXTPLAIN = "text/plain";
static char* CT_IMGGIF = "image/gif";
static char* CT_IMGJPEG = "image/jpeg";
static char* CT_IMGPNG = "image/png";
static char* CT_IMGBMP = "image/x-xbitmap";

static char *str_toupper(char *fr)
{
	char *rval;
	int i;

	rval = strdup(fr);
	for (i = 0; i < strlen(rval); i++) {
		if (islower(rval[i]))
			rval[i] = toupper(rval[i]);
	}
	return rval;
}

static char *get_content_type(char *path)
{
	char *up_path;
	char *rval;

	up_path = str_toupper(path);
	if (strstr(up_path,".GIF")) rval = CT_IMGGIF;
	else if (strstr(up_path,".JPEG") || 
		 strstr(up_path,".JPG")) rval = CT_IMGJPEG;
	else if (strstr(up_path,".PNG")) rval = CT_IMGPNG;
	else if (strstr(up_path,".BMP")) rval = CT_IMGBMP;
	else if (strstr(up_path,".TXT")) rval = CT_TEXTPLAIN;
	else rval = CT_TEXTHTML;

	free(up_path);
	return rval;
}

request_t new_request(char *raw, request_t rq)
{
	RQ_TYPE rqt;
	char *end_headers;

	/* make sure we can handle the request */
	rqt = get_rq_type(raw);
	if (rqt == RQ_UNKNOWN) return NULL;

	if (!rq) rq = ag_malloc(rq_size);
	rq->destroy = destroy_request;
	rq->rtype = rqt;
	rq->path = get_path(raw);
	rq->content_type = get_content_type(rq->path);
	rq->prot = get_prot(raw);
	rq->body = raw;
	get_headers(rq, raw);

	/* dup body: */
	end_headers = strstr(raw, "\r\n\r\n");
	end_headers += 4;
	rq->body = strdup(end_headers);
	
	return rq;
}

int destroy_request(void *arg)
{
	request_t rq = arg;
	int i;
	
	if (rq->prot) free(rq->prot);
	if (rq->path) free(rq->path);
	if (rq->body) free(rq->body);
	/*if (rq->content_type) free(rq->content_type);*/

	for (i = 0; i < H_MAX; i++) {
		if (rq->headers[i]) free(rq->headers[i]);
	}
#if 0
	for (i = 0; i < rq->numheaders; i++) {
		if (rq->headers[i].name) free(rq->headers[i].name);
		if (rq->headers[i].value) free(rq->headers[i].value);
	}
#endif

	/* dangerous? */
	free(rq);
	rq = NULL;
	return 1;
}
