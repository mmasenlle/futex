
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "config.h"
#include "logger.h"
#include "utils.h"
#include "globals.h"
#include "State.h"


#define BOX_STATE_FNAME DATOS_PATH "state.dat"

struct box_state_t {
	int inputs[64]; //iobox 31..46
	int outputs[64];
	int ifstate[32]; // up to 20 desks
	int mstate[8];
};

static int fd = -1;
static struct box_state_t *box_st = NULL;


bool State::init(bool bwrite)
{
	if (bwrite) {
//		if((fd = ::open(BOX_STATE_FNAME, O_RDWR | O_CREAT, 0600)) == -1)
		if((fd = ::shm_open("/box_state", O_RDWR | O_CREAT, 0600)) == -1)
		{
			SELOG("State::init(%d) -> Cannot open '%s'", bwrite, BOX_STATE_FNAME);
			return false;
		}
		DLOG("State::init() -> truncating to %d", sizeof(box_state_t));
		if (ftruncate(fd, sizeof(box_state_t)) == -1) {
			SELOG("State::init() -> Cannot ftruncate (%d)", sizeof(box_state_t));
			::close(fd); fd = -1;
			return false;
		}
		box_st = (struct box_state_t *)mmap(NULL, sizeof(box_state_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	} else {
//		if((fd = ::open(BOX_STATE_FNAME, O_RDONLY)) == -1)
		if((fd = ::shm_open("/box_state", O_RDONLY, 0600)) == -1)
		{
			SELOG("State::init(%d) -> Cannot open '%s'", bwrite, BOX_STATE_FNAME);
			return false;
		}
		box_st = (struct box_state_t *)mmap(NULL, sizeof(box_state_t), PROT_READ, MAP_SHARED, fd, 0);
	}
	DLOG("State::init(%d) -> mem %p, size %d", bwrite, box_st, sizeof(box_state_t));
	if (box_st == MAP_FAILED) {
		SELOG("State::init(%d) -> Cannot mmap '%s' (%d)", bwrite, BOX_STATE_FNAME, sizeof(box_state_t));
		box_st = NULL;
		::close(fd); fd = -1;
		return false;
	}
	return true;
}

void State::close()
{
	if (munmap(box_st, sizeof(box_state_t)) == -1)
		SELOG("State::close() -> Cannot munmap");
	if (::close(fd))
		SELOG("State::close() -> Cannot close");
	box_st = NULL;
	fd = -1;
}
	
void State::set_input(int i, int v)
{
	if (LIKELY(box_st && (i < COUNTOF(box_st->inputs)))) {
		box_st->inputs[i] = v;
		if (i > box_st->inputs[0])
			box_st->inputs[0] = i; //last element in use
	}
}

void State::set_output(int i, int v)
{
	if (LIKELY(box_st && (i < COUNTOF(box_st->outputs)))) {
		box_st->outputs[i] = v;
		if (i > box_st->outputs[0])
			box_st->outputs[0] = i; //last element in use
	}
}

void State::set_ifstate(int i, int v)
{
	if (LIKELY(box_st && (i < COUNTOF(box_st->ifstate)))) {
		box_st->ifstate[i] = v;
		if (i > box_st->ifstate[0])
			box_st->ifstate[0] = i; //last element in use
	}
}

void State::set_mstate(int i, int v)
{
	if (LIKELY(box_st && (i < COUNTOF(box_st->mstate)))) {
		box_st->mstate[i] = v;
		if (i > box_st->mstate[0])
			box_st->mstate[0] = i; //last element in use
	}
}

int State::get_input(int i)
{
	if (LIKELY(box_st && (i < COUNTOF(box_st->inputs))))
		return box_st->inputs[i];
	return -1;
}

int State::get_output(int i)
{
	if (LIKELY(box_st && (i < COUNTOF(box_st->outputs))))
		return box_st->outputs[i];
	return -1;
}

int State::get_ifstate(int i)
{
	if (LIKELY(box_st && (i < COUNTOF(box_st->ifstate))))
		return box_st->ifstate[i];
	return -1;
}

int State::get_mstate(int i)
{
	if (LIKELY(box_st && (i < COUNTOF(box_st->mstate))))
		return box_st->mstate[i];
	return -1;
}
