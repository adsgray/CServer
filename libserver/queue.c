/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#include <stdlib.h>
#include "queue.h"
#include "ag.h"

static int default_destroy_q(void *arg)
{
	queue_t self = arg;
	/* we don't really know what is stored in us, so we
	   can't exactly walk along and free it. Hmmmm.
	   Unless a destroy method is added to queue_connector_t.
	*/
	free(self);
	self = NULL;
	return 0;
}

void default_push(queue_t self, void *cdata)
{
	queue_connector_t *data = cdata;
	
	/* nothing in q */
	if (!self->first) {
		self->first = self->last = data;
		/* paranoia */
		data->prev = data->next = NULL;
	}
	/* general case */
	else {
		data->prev = self->last;
		data->next = NULL;
		self->last->next = data;
		self->last = data;
	}
}

void *default_pop(queue_t self)
{
	queue_connector_t *rval; 
	
	if (!self->first) return NULL;
	
	rval = self->first;

	/* special case. one element in queue */
	if (self->first == self->last) {
		self->first = self->last = NULL;
	}
	/* general case */
	else {
		self->first = self->first->next;
		self->first->prev = NULL;
	}

	/* paranoia */
	rval->next = rval->prev = NULL;
	return rval;
	
}


#define queue_size sizeof(struct _queue_t)
queue_t new_queue()
{
	queue_t q;

	q = ag_malloc(queue_size);
	q->push = default_push;
	q->pop = default_pop;
	q->destroy = default_destroy_q;

	return q;
}
