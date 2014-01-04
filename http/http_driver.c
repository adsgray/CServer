
/* attempt at a http driver... uses defaults mostly */

#include "http.h"
#include "server.h"
#include "handler.h"
#include "connection.h"
#include "request.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static size_t spew_file(int fd, char *fname);
static void say_server_headers(http_t h, int fd, request_t rq);
static int handle_get(handler_t hd, request_t rq);
static void rq_summary(request_t rq);

/*
 * Should add functions here to open up files
 * and spew them out over con->sk.
 * Done.
 *
 * Now, should add code to interpret ~username paths...
 */

/* have a 404 handler, too... */

/* returns number of bytes written */
static size_t spew_file(int fd, char *fname)
{
	FILE *F;
#define BUF_SIZ 8192
	char buf[BUF_SIZ];
	size_t n;
	size_t r = 0;
	struct stat stbuf;

	/* check if the file is actually a directory. 
	 * reject directories? or look for index.[s]html ? configurable?
	 */
	if (stat(fname, &stbuf) == -1) {
		memset((void *)buf, 0, BUF_SIZ);	
		sprintf(buf, "404 \r\n%s not found\n", fname);
		(void)write(fd, (void *)buf, strlen(buf));
		return 0;
	}

	/* we are modifying a parameter... bad */
	if (S_ISDIR(stbuf.st_mode)) {
		strcat(fname, "index.html");
	}

	F = fopen(fname, "r");
	if (!F) {
		fprintf(stderr, "spew_file: cannot open %s\n", fname);
		return 0;
	}

	do {
		memset((void *)buf, 0, BUF_SIZ);	
		n = fread((void *)buf, 1, BUF_SIZ, F);
		if (n > 0) (void)write(fd, (void *)buf, n);
		r += n;
	} while (!feof(F));

	fprintf(stderr, "spew_file: done spewing %s (%d bytes)\n",fname,r);

	return r;
}

/* should have a "response_t" object that has:
   200/404/blah server code
   prot version
   content type
   
   This object is then passed to say_server_headers...
*/
#define SAY_REPLY(FD,R) if ( (strlen(R) > 0) && (write(FD, (void *)R, strlen(R)) <= 0) ) goto out
static void say_server_headers(http_t h, int fd, request_t rq)
{
	SAY_REPLY(fd, "HTTP/1.0 200 OK\r\n");
	SAY_REPLY(fd, "server: ");
	SAY_REPLY(fd, h->_svr.name);
	SAY_REPLY(fd, "\r\n");
	SAY_REPLY(fd, "content-type: ");
	SAY_REPLY(fd, rq->content_type);
	SAY_REPLY(fd, "\r\n");
	SAY_REPLY(fd, "\r\n");

 out:
}

static void rq_summary(request_t rq)
{
	fprintf(stderr, "%s : %s : %s : %s \n", 
		RQ_STRING[rq->rtype],
		rq->path,
		rq->prot,
		rq->content_type);
	fprintf(stderr, "-- \n");
}

static int handle_get(handler_t hd, request_t rq)
{
	connection_t con = hd->con;
	http_t h = (http_t)hd->svr;
	char *pos;
	char buf[256];
	int n;

	rq_summary(rq);

	/* construct file name from docroot */
	memset((void *)buf, 0, 256);	
	sprintf(buf, "%s%s", h->docroot, rq->path);

	say_server_headers(h, con->sk, rq);
	(void)spew_file(con->sk, buf);

	return 1;
}

int main(int argc, char **argv)
{
	http_t h;

	h = new_http(NULL);
	h->docroot = "/tmp/fiftymegs_html/fiftymegs_html";
	h->get_handler = handle_get;
	h->_svr.name = "HTTP Driver";
	h->_svr.port = atoi(argv[1]);
	h->_svr.backlog = 5;
	h->_svr.numthreads = 15;
	h->_svr.maxcon = 10;

	/* start the ball rolling... 
	 * Notice that we are passing a http_t to a function
	 * that is expecting a server_t.
	 *
	 * Holy Polymorphism, batman!
	 *
	 * This works because the first member of a http_t
	 * is a server_t.
	 *
	 * Anyone who says you cannot do object oriented
	 * programming in C is being foolish.
	 */
	(void)h->_svr.start((void*)h);

	/* never gets here */
	return 0;
}
