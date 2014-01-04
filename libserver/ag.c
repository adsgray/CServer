/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "ag.h"

void *ag_malloc(size_t sz)
{
	void *r;

	r = calloc(1,sz);

	if (!r) {
		fprintf(stderr, "malloc [%d] failed\n", sz);
		abort();
	}

	return r;
}
