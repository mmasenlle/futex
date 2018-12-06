
#include <stdio.h>
#include <stdlib.h>
#include "box_futex.h"

#define IBUF_LEN 128

static int l1 = 10, l2 = 10, bf = 1;
volatile int *var = NULL;

int operation_a(int v, int n)
{
	time(NULL);
	return (v + n);
}

int operation_b(int v, int n)
{
	time(NULL);
	return (v - n);
}


void p_run(int n)
{
	int i, j, k;
	struct box_futex_handler_t bf_h;
	fprintf(stderr, "p_run(%d) start\n", n);
	if(bf) if(box_futex_init(0, &bf_h) != 0) {
		perror("box_futex_init");
		exit(-1);
	}
	for(j = 0; j < l1; j++) {
		for(k = 0; k < l2; k++) {
			if(bf) box_futex_wlock(&bf_h);
			*var = operation_a(*var, n);
			if(bf) box_futex_unlock(&bf_h);
		}
		for(k = 0; k < l2; k++) {
			if(bf) box_futex_wlock(&bf_h);
			*var = operation_b(*var, n);
			if(bf) box_futex_unlock(&bf_h);
		}
	}
	if(bf) if(box_futex_destroy(&bf_h) != 0) perror("box_futex_destroy");
	fprintf(stderr, "p_run(%d) done.\n", n);
}

void usage()
{
	fprintf(stderr, "usage: bf_test [-n l1] [-l l2] [-b b] [-p pn]\n\n");
	exit(-1);
}

int main(int argc, char *argv[])
{
	int i, n = 8, fd;
	int primes[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101 };

	for(i = 0; i < argc; i++) {
		if(argv[i][0] == '-') {
			switch(argv[i][1]) {
			case 'n': if(++i >= argc) usage(); l1 = atoi(argv[i]); break;
			case 'l': if(++i >= argc) usage(); l2 = atoi(argv[i]); break;
			case 'p': if(++i >= argc) usage(); n = atoi(argv[i]); break;
			case 'b': if(++i >= argc) usage(); bf = atoi(argv[i]); break;
			}
		}
	}
	if(n > (sizeof(primes)/sizeof(primes[0]))) n = (sizeof(primes)/sizeof(primes[0]));
	fprintf(stderr, "\nbf_test: %d:%d times, %d processes, bf_lock %s\n\n", l1, l2, n, bf ? "active" : "disabled");
	
	fd = open("file_int.data", O_RDWR | O_CREAT, 0660);
	if(fd == -1) {
		perror("open");
		exit(-1);
	}
	if(ftruncate(fd, sizeof(int)) == -1) {
		perror("mmap");
		exit(-1);
	}
	var = (int*)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(var == MAP_FAILED) {
		perror("mmap");
		exit(-1);
	}
	
	for(i = 0, *var = 0; i < n; i++) {
		fprintf(stderr, "bf_test: creating process %d\n", i);
		if(fork() == 0) {
			printf("bf_test(%d): born\n", i);
			p_run(primes[i]);
			exit(0);
		}
	}
	fprintf(stderr, "bf_test: waiting processes ...\n");
	while(wait(NULL) > 0);
	fprintf(stderr, "bf_test: all process terminated\n");

	fprintf(stderr, "\nbf_test: var is %d\n\n", *var);
	exit(0);
}

