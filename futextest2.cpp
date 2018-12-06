
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include "box_futex.h"

void catch_signal(int s)
{
	printf("\n\n *** signal %d *** \n\n\n", s);
}

int main(int argc, char *argv[])
{
	int bf_class = argc > 1 ? atoi(argv[1]) : 0;
	
	signal(SIGPIPE, catch_signal);

	struct box_futex_handler_t bfh;
	printf("box_futex_init ...\n");
	if(box_futex_init(bf_class, &bfh) == -1)
	{
		perror("box_futex_init ");
		_exit(-1);
	}
	printf("... done\n");
	getchar();
	printf("box_futex_rlock ...\n");
	box_futex_rlock(&bfh);
	printf("... done\n");
	getchar();
	printf("box_futex_unlock ...\n");
	box_futex_unlock(&bfh);
	printf("... done\n");
	getchar();
	printf("box_futex_wlock ...\n");
	box_futex_wlock(&bfh);
	printf("... done\n");
	getchar();
	printf("box_futex_unlock ...\n");
	box_futex_unlock(&bfh);
	printf("... done\n");
	getchar();
	printf("box_futex_destroy ...\n");
	if(box_futex_destroy(&bfh) == -1)
	{
		perror("box_futex_destroy ");
		_exit(-1);
	}
	printf("... done\n");

	printf("All done\n\n");
}
