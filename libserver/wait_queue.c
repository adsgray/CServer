/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "wait_queue.h"
#include "handler.h"
#include "ag.h"

#define wq_size sizeof(struct _wait_queue_t)

/*
 * If pthread_cond_broadcast is used, *all* threads waiting on the
 * condition variable are woken up, causing a "stampede" when only
 * one or two of the threads will actually get a connection.
 *
 * If pthread_cond_signal is used, only one thread is woken up.
 * That thread then signals the condition variable again.
 * If there is no connection to be processed, no other threads
 * are signaled. So at most one thread which has nothing
 * to do is activated.
 *
 * If this #define is commented, the first method (broadcast) is
 * used. If the #define is uncommented, the second method (signal)
 * is used.
 */
#define USE_SIGNAL

static void default_wait_on(wait_queue_t self, handler_t hd);
static int default_add_con(wait_queue_t self, connection_t con);
static int default_destroy_wq(void *arg);

/* allocate and return a default wait_queue */
wait_queue_t new_wait_queue()
{
	wait_queue_t wq;
	
	wq = ag_malloc(wq_size);
	wq->cons_queue = new_queue();
	wq->wait_on = default_wait_on;
	wq->add_con = default_add_con;
	wq->destroy = default_destroy_wq;
	pthread_cond_init(&wq->w, NULL);
	pthread_mutex_init(&wq->m, NULL);

	return wq;
}

static int default_destroy_wq(void *arg)
{
	int rval = 0;
	wait_queue_t wq = arg;

	rval += wq->cons_queue->destroy(wq->cons_queue);
	free(wq);
	wq = NULL;

	return rval;
}

/* The default add_con method.
 * Adds con to the queue and wakes up a handler(s)
 */
static int default_add_con(wait_queue_t self, connection_t con)
{
	queue_t q = self->cons_queue;

	pthread_mutex_lock(&self->m);

	if (self->num >= self->max) {
		fprintf(stderr, "default_add_on: %d connections reached\n",
				self->num);
		self->num_refused++;
		pthread_mutex_unlock(&self->m);
		return 0;
	}

	q->push(q, con);
	self->num++;
	pthread_mutex_unlock(&self->m);

	/* could maybe make this a _signal, then see [2] below */
#ifdef USE_SIGNAL
	pthread_cond_signal(&self->w);
#else
	pthread_cond_broadcast(&self->w);
#endif

	return 1;
}



/* The default wait_on method.
 * Called by a handler thread.
 * The handler waits on the condition variable and
 * handles any connections that are added to the queue "self"
 */
static void default_wait_on(wait_queue_t self, handler_t hd)
{
	connection_t con;
	queue_t q = self->cons_queue;

	pthread_mutex_lock(&self->m);

	for (;;) {

		/* only wait if there is nothing to process */
		if (self->num == 0) {
			hd->hd_state = HD_WAITING;
			pthread_cond_wait(&self->w, &self->m);
		}

		/* now that we are awake, check if there is a
		   connection to handle. Note that we hold the lock. */
		if (self->num > 0) {
		  con = q->pop(q);
		  self->num--;

		  pthread_mutex_unlock(&self->m);
#ifdef USE_SIGNAL
		  pthread_cond_signal(&self->w);
#endif
		  hd->con = con;
		  hd->handle_con(hd);
		  pthread_mutex_lock(&self->m);
		}
		/* [2] then somehow have the thread in here _signal it again */
	}
}
