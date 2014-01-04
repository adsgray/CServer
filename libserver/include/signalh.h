#ifndef _SIGNALH_H_
#define _SIGNALH_H_

#include <signal.h>

/* signal handler object. starts a thread to handle
   signals for the process... */

typedef struct _signalh_t *signalh_t;
typedef void (*signal_handler_t)(int);

struct _signalh_t {
	signal_handler_t handler;
	int (*start)(signalh_t); /* installs your handler etc */
};

inside start:

	sigset_t mask;
  	struct sigaction act;

  act.sa_handler = signal_handler;
  act.sa_mask    = empty_mask;
  act.sa_flags   = 0;
  
  sigaction(SIGINT,  &act, 0);
  sigaction(SIGHUP,  &act, 0);
  sigaction(SIGSEGV, &act, 0);
  sigaction(SIGABRT, &act, 0);
#endif
