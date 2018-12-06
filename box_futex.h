#ifndef BOX_FUTEX_H_
#define BOX_FUTEX_H_

#define BOXFUTEX_MNAME "box_futex"
#define BOXFUTEX_MAXBFUTEX 8
#define BOXFUTEX_MEMGAP 100
#define BOXFUTEX_ULOCK_ADDR(_mem, _nbftx) ((struct box_futex_ulock_t *)((unsigned long)(_mem) + (_nbftx * BOXFUTEX_MEMGAP)))
#define BOXFUTEX_MEMSIZE (BOXFUTEX_MAXBFUTEX * BOXFUTEX_MEMGAP)

#define BOXFUTEX_WAITMARK    0x80000000
#define BOXFUTEX_WRITING     0x40000000
#define BOXFUTEX_IDMARK(_id) (1 << _id)

enum {
	BOXFUTEX_REQ_READ,
	BOXFUTEX_REQ_WRITE,
	BOXFUTEX_REQ_LEAVE,
};

#define _bf_likely(x)	__builtin_expect(!!(x), 1)
#define _bf_unlikely(x)	__builtin_expect(!!(x), 0)

static inline unsigned long _bf_cmpxchg(volatile unsigned long *a, unsigned long o, unsigned long n)
{
	unsigned long prev;
	__asm__ __volatile__("lock ; cmpxchgl %1,%2"
			     : "=a"(prev)
			     : "r"(n), "m"(*a), "0"(o)
			     : "memory");
	return prev;
}

struct box_futex_ulock_t {
	unsigned long status;
};

#ifndef __KERNEL__

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define BOXFUTEX_DEVICE_BASE "/dev/" BOXFUTEX_MNAME "%d"

struct box_futex_handler_t {
	int id;
	int fd;
	void *mem;
	struct box_futex_ulock_t *ulock;
};

static inline int box_futex_init(int bftx_class, struct box_futex_handler_t *handler)
{
	char buf[64]; int flags;
	if(bftx_class >= BOXFUTEX_MAXBFUTEX) goto end_error;
	snprintf(buf, sizeof(buf), BOXFUTEX_DEVICE_BASE, bftx_class);
	
	if((handler->fd = open(buf, O_RDWR)) == -1)
		goto end_error;

	if(((flags = fcntl(handler->fd, F_GETFD)) == -1) ||
			(fcntl(handler->fd, F_SETFD, flags | FD_CLOEXEC) == -1)) {
		close(handler->fd);
		goto end_error;
	}
	if((handler->id = read(handler->fd, NULL, 0)) == -1) {
		close(handler->fd);
		goto end_error;
	}
	if((handler->mem = mmap(NULL, BOXFUTEX_MEMSIZE,
			PROT_READ | PROT_WRITE, MAP_SHARED, handler->fd, 0)) == MAP_FAILED) {
		close(handler->fd);
		goto end_error;
	}
	handler->ulock = BOXFUTEX_ULOCK_ADDR(handler->mem, bftx_class);
	return 0;

end_error:
	handler->id = handler->fd = -1;
	handler->mem = handler->ulock = NULL;
	return -1;
}

static inline int box_futex_destroy(struct box_futex_handler_t *handler)
{
	int rm = munmap(handler->mem, BOXFUTEX_MEMSIZE);
	int rc = close(handler->fd);
	handler->id = handler->fd = -1;
	handler->mem = handler->ulock = NULL;
	return (rm < 0 || rc < 0) ? -1 : 0;
}


static inline void box_futex_rlock(const struct box_futex_handler_t *handler)
{
	for (;;) {
		unsigned long status = handler->ulock->status;
		if(_bf_likely(status < BOXFUTEX_WRITING)) {
			unsigned long old_status = _bf_cmpxchg(&handler->ulock->status, status, status | BOXFUTEX_IDMARK(handler->id));
			if(_bf_likely(status == old_status)) return;
		} else {
			unsigned long old_status = _bf_cmpxchg(&handler->ulock->status, status, status | BOXFUTEX_WAITMARK);
			if(_bf_likely(status == old_status)) {
				if(write(handler->fd, NULL, BOXFUTEX_REQ_READ) == -1) {
					if(errno != EINTR) exit(-1);
				} else {
					return;
				}
			}
		}
	}
}

static inline void box_futex_wlock(const struct box_futex_handler_t *handler)
{
	for (;;) {
		unsigned long status = handler->ulock->status;
		if(_bf_likely(status == 0)) {
			unsigned long old_status = _bf_cmpxchg(&handler->ulock->status,
									status, status | (BOXFUTEX_WRITING | BOXFUTEX_IDMARK(handler->id)));
			if(_bf_likely(status == old_status)) return;
		} else {
			unsigned long old_status = _bf_cmpxchg(&handler->ulock->status, status, status | BOXFUTEX_WAITMARK);
			if(_bf_likely(status == old_status)) {
				if(write(handler->fd, NULL, BOXFUTEX_REQ_WRITE) == -1) {
					if(errno != EINTR) exit(-1);
				} else {
					return;
				}
			}
		}
	}
}

static inline void box_futex_unlock(const struct box_futex_handler_t *handler)
{
	for (;;) {
		unsigned long status = handler->ulock->status;
		unsigned long old_status = _bf_cmpxchg(&handler->ulock->status,
										status, status & ~(BOXFUTEX_WRITING | BOXFUTEX_IDMARK(handler->id)));
		if(_bf_likely(status == old_status)) {
			if(_bf_unlikely((status & ~(BOXFUTEX_WRITING | BOXFUTEX_IDMARK(handler->id))) == BOXFUTEX_WAITMARK))
				write(handler->fd, NULL, BOXFUTEX_REQ_LEAVE);
			return;
		}
	}
}

#endif

#endif /*BOX_FUTEX_H_*/
