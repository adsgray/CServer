/*
 * Copyright (c) 2000 Andrew Gray 
 * <agray@alumni.uwaterloo.ca>
 *
 * See COPYING for details.
 *
 */

#include <stdio.h>
#include "smtp_server.h"

int main(int argc, char **argv)
{
  struct _smtp_server_options svrop = {
    0, /* min_delay */
    5, /* max_delay */
    4, /* percent_4xx */
    4, /* percent_5xx */
    3 /* percent_dropped */
  };
	printf("Hello World!\n");

	(void) smtp_server((void *)&svrop);

	return 0; /* shut up warnings */

}
