/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#ifndef _SMTP_SERVER_H_
#define _SMTP_SERVER_H_

typedef struct _smtp_server_options *smtp_server_options_t;


struct _smtp_server_options {
  int min_delay;  /* minimum delay before responding (usually 0) */
  int max_delay;  /* max */
  int percent_4xx; /* probability of 4xx response */
  int percent_5xx; /* probability of 5xx response */
  int percent_dropped; /* probability of dropping the connection NOW */
};

void *smtp_server(void *);

#endif
