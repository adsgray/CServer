/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#ifndef _QUEUE_H_
#define _QUEUE_H_

typedef struct _queue_t *queue_t;

/* everything which is added to the queue must have this
   as its first member */
typedef struct _queue_connector {
	void *prev, *next;
	/* int (*destroy)(void *); */
} queue_connector_t;

struct _queue_t {
	queue_connector_t *first, *last;
	void (*push)(queue_t, void *); /* add element to queue */
	void *(*pop)(queue_t);        /* get element from queue */
	int (*destroy)(void *);       /* "destructor" */
};

queue_t new_queue();

#endif
