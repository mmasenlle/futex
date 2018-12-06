
#include <stdlib.h>
#include <stdio.h>
#include "box_futex.h"

int main(int argc, char *argv[])
{
	int bf_class = argc > 1 ? atoi(argv[1]) : 0;
#if 0
	printf("BOXFUTEX_WFIRST  0x%08x\n", BOXFUTEX_WFIRST);
	printf("BOXFUTEX_RMASK   0x%08x\n", BOXFUTEX_RMASK);
	printf("BOXFUTEX_WMASK   0x%08x\n", BOXFUTEX_WMASK);
	printf("BOXFUTEX_RPOS(5) 0x%08x\n", BOXFUTEX_RPOS(5));
	printf("BOXFUTEX_WPOS(5) 0x%08x\n", BOXFUTEX_WPOS(5));
	printf("BOXFUTEX_POSS(5) 0x%08x\n", BOXFUTEX_POSS(5));
#endif
#if 0
	unsigned long a = 0x0f0;
	unsigned long v = bf_atomic_clear_mask(&a, 0x080);
	printf("a: 0xf0 v: 0x%x a: 0x%x\n", v, a);
	v = bf_atomic_set_mask(&a, 0x080);
	printf("v: 0x%x a: 0x%x\n", v, a);
#endif
#if 1
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
	if(box_futex_rlock(&bfh) == -1)
	{
		perror("box_futex_rlock ");
		_exit(-1);
	}
	printf("... done\n");
	getchar();
	printf("box_futex_unlock ...\n");
	if(box_futex_unlock(&bfh) == -1)
	{
		perror("box_futex_unlock ");
		_exit(-1);
	}
	printf("... done\n");
	getchar();
	printf("box_futex_wlock ...\n");
	if(box_futex_wlock(&bfh) == -1)
	{
		perror("box_futex_wlock ");
		_exit(-1);
	}
	printf("... done\n");
	getchar();
	printf("box_futex_unlock ...\n");
	if(box_futex_unlock(&bfh) == -1)
	{
		perror("box_futex_unlock ");
		_exit(-1);
	}
	printf("... done\n");
	getchar();
	printf("box_futex_destroy ...\n");
	if(box_futex_destroy(&bfh) == -1)
	{
		perror("box_futex_destroy ");
		_exit(-1);
	}
	printf("... done\n");
#endif	
	printf("All done\n\n");

}
