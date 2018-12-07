
#include <time.h>
#include "config.h"
#include "globals.h"
#include "logger.h"
#include "utils.h"
#include "futex/box_futex.h"
#include "Timetable.h"

#define TIMETABLE_FNAME DATOS_PATH "timetable.dat"
#define TIMETABLE_FUTEX 1

#define N_BITS_HASH 13
#define HASH_SIZE   (1 << N_BITS_HASH)
#define HASH_MASK   (HASH_SIZE - 1)
#define MEMSIZE_MIN (sizeof(timetable_table_t) + (16 * 1024))

struct timetable_element_t
{
	union {
		struct {
			__s64 id;
			int next;
		} id;
		__u32 data[3];
	} data;
	int next;
};

struct timetable_table_t
{
	int first[HASH_SIZE];
	timetable_element_t elements[0];
};

#define TT_TABLE (((struct timetable_table_t *)hashMap.get_mem()))
#define CURRENT_LAST (int)((hashMap.get_size() - sizeof(timetable_table_t)) / sizeof(timetable_element_t))
#define REQUIRED_SIZE(_i) ((((_i) + 1) * sizeof(timetable_element_t)) + sizeof(timetable_table_t))
#define IDD(_id,_pn) (((_id & ~HASH_MASK) >> 1) | _pn)
#define ID(_id,_cur) (((_id & ~(HASH_MASK >> 1)) << 1) | _cur)
#define IDINF(_id) (_id & HASH_MASK)


HashMap Timetable::hashMap;
static struct box_futex_handler_t futex;


bool Timetable::locate()
{
	prev = 0; cur = TT_TABLE->first[IDINF(id)];
	__s64 ida = IDD(id, perm_num), prev_ida = -1;
	while (cur)	{
		if (UNLIKELY(cur >= CURRENT_LAST))
			if (!hashMap.grow())
				return false;
		__s64 cur_ida = (TT_TABLE->elements[cur].data.id.id);
		if (cur_ida >= ida || cur_ida <= prev_ida)
			return (cur_ida == ida);
		prev_ida = cur_ida;
		prev = cur;
		cur = TT_TABLE->elements[cur].next;
	}
	return false;
}

int Timetable::_alloc()
{
	int first = TT_TABLE->elements[0].next ? : 1;
	if (UNLIKELY(first >= CURRENT_LAST)) {
		if(!hashMap.grow(REQUIRED_SIZE(first + 1))) return 0;
	}
	TT_TABLE->elements[0].next = TT_TABLE->elements[first].next ? : (first + 1);

	return first;
}

void Timetable::clear_intervals(int tnext)
{
	while (tnext) {
		int nnext = TT_TABLE->elements[tnext].next;
		TT_TABLE->elements[tnext].next = TT_TABLE->elements[0].next;
		TT_TABLE->elements[0].next = tnext;
		tnext = nnext;
	}
}

int Timetable::set_intervals()
{
	int first = 0, j = 0, current = 0;
	if (sdata) {
		for (std::set<unsigned long>::iterator i = sdata->begin(); i != sdata->end(); i++) {
			if (j == 0) {
				int nel = _alloc();
				if (nel) {
					memset(&TT_TABLE->elements[nel], 0, sizeof(timetable_element_t));
					if (!first) first = nel;
					else TT_TABLE->elements[current].next = nel;
					current = nel;
				}
			}
			if (!current) break;
			TT_TABLE->elements[current].data.data[j] = *i;
			j = (j + 1) % COUNTOF(TT_TABLE->elements[current].data.data);
		}
	}
	return first;
}

bool Timetable::get()
{
	if (LIKELY(hashMap.get_mem())) {
		box_futex_rlock(&futex);
		if (locate()) {
			int tnext = TT_TABLE->elements[cur].data.id.next;
			while (tnext) {
				tdata_t *td = (tdata_t *)TT_TABLE->elements[tnext].data.data;
				for (int i = 0; i < 3; i++) {
					if(td[i].mB == td[i].mE) break;
					vdata.push_back(td[i]);
				}
				tnext = TT_TABLE->elements[tnext].next;
			}
			box_futex_unlock(&futex);
			return true;
		}
		box_futex_unlock(&futex);
	}
	return false;
}

void Timetable::del()
{
	if (LIKELY(hashMap.get_mem())) {
		box_futex_wlock(&futex);
		if (locate()) {
			if (!prev)
				TT_TABLE->first[IDINF(id)] = TT_TABLE->elements[cur].next;
			else
				TT_TABLE->elements[prev].next = TT_TABLE->elements[cur].next;
			TT_TABLE->elements[cur].next = TT_TABLE->elements[0].next;
			TT_TABLE->elements[0].next = cur;
			clear_intervals(TT_TABLE->elements[cur].data.id.next);
		}
		box_futex_unlock(&futex);
	}
}

void Timetable::put()
{
	if (LIKELY(hashMap.get_mem())) {
		box_futex_wlock(&futex);
		if (!locate()) {
			if ((cur = _alloc())) {
				TT_TABLE->elements[cur].data.id.next = 0;
				if (prev) {
					TT_TABLE->elements[cur].next = TT_TABLE->elements[prev].next;
					TT_TABLE->elements[prev].next = cur;
				} else {
					TT_TABLE->elements[cur].next = TT_TABLE->first[IDINF(id)];
					TT_TABLE->first[IDINF(id)] = cur;
				}
			}
		}
		if (cur) {
			clear_intervals(TT_TABLE->elements[cur].data.id.next);
			int next = set_intervals(); // set_intervals changes TT_TABLE
			TT_TABLE->elements[cur].data.id.next = next;
			TT_TABLE->elements[cur].data.id.id = IDD(id, perm_num);
		}
		box_futex_unlock(&futex);
	}
}

void Timetable::init()
{
	if(box_futex_init(TIMETABLE_FUTEX, &futex) == -1)
	{
		SELOG("Timetable::init() -> Cannot init futex %d", TIMETABLE_FUTEX);
		return;
	}
	box_futex_wlock(&futex);
	hashMap.open(TIMETABLE_FNAME, MEMSIZE_MIN);
	box_futex_unlock(&futex);
	DLOG("Timetable::init() -> mem: %p size: %d (min: %d)", hashMap.get_mem(), hashMap.get_size(), MEMSIZE_MIN);
}

void Timetable::end()
{
	if(hashMap.get_mem()) hashMap.close();
	if(box_futex_destroy(&futex) == -1)
	{
		SELOG("Timetable::end() -> box_futex_destroy");
	}
}

Timetable::Timetable(perm_ID_t num, int pn, int lt) : sdata(NULL)
{
	id = num;
	perm_num = pn;
	if (lt == LECTYPE_BOARDING) perm_num += 4;
}

Timetable::~Timetable()
{
	if(sdata) delete sdata;
}

void Timetable::addInterval(int wdB, int wdE, int hB, int hE, int mB, int mE)
{
	if(!sdata) sdata = new std::set<unsigned long>;

	if (wdB > wdE) {
		tdata_t td;
		td.mB = convert(wdB, hB, mB);
		td.mE = convert(8, 0, 0);
		unsigned long *d = (unsigned long *)&td;
		sdata->insert(*d);
		wdB = 0;
	}
	tdata_t td;
	td.mB = convert(wdB, hB, mB);
	td.mE = convert(wdE, hE, mE);
	unsigned long *d = (unsigned long *)&td;
	sdata->insert(*d);
}

bool Timetable::match()
{
	if(!vdata.size()) return true;
	time_t now = time(NULL);
	struct tm ttm; localtime_r(&now, &ttm);
	unsigned short mnow = convert(ttm.tm_wday + 1, ttm.tm_hour, ttm.tm_min);
	for (int i = 0; i < (int)vdata.size(); i++) {
		if(vdata[i].mB <= mnow && mnow <= vdata[i].mE) return true;
	}
	return false;
}

void Timetable::merge()
{
	if(!sdata && vdata.size()) sdata = new std::set<unsigned long>;
	for (int i = 0; i < (int)vdata.size(); i++) {
		sdata->insert(*(unsigned long*)&vdata[i]);
	}
}

void Timetable::dump(std::string *buf)
{
	for (int i = 0; i < (int)vdata.size(); i++) {
		int d1 = vdata[i].mB / (60 * 24);
		int h1 = (vdata[i].mB - (d1*24*60)) / 60;
		int m1 = vdata[i].mB %  60;
		int d2 = vdata[i].mE / (60 * 24);
		int h2 = (vdata[i].mE - (d2*24*60)) / 60;
		int m2 = vdata[i].mE %  60;
		char buffer[64];
		int n = snprintf(buffer, sizeof(buffer), "%d-%02d:%02d..%d-%02d:%02d,", d1, h1, m1, d2, h2, m2);
		buf->append(buffer, n);
	}
}

unsigned short Timetable::convert(int wd, int h, int m)
{
	return (m + (60 * (h + (24 * wd))));
}

bool Timetable::commitIntervals()
{
	int ssize = 0;
	if(sdata) ssize = sdata->size();
	if(ssize != (int)vdata.size()) return true;
	for (int i = 0; i < (int)vdata.size(); i++) {
		if(sdata->find(*(unsigned long*)&vdata[i]) == sdata->end()) return true;
	}
	return false;
}

void Timetable::stats(std::string *sbuf)
{
	char buf[512];
	int idmax = 0, max = 0, zeros = 0, total = 0;
	int tmax = 0, tzeros = 0, ttotal = 0;
	box_futex_rlock(&futex);
	for (int i = 0; i < HASH_SIZE; i++) {
		int count = 0;
		int next = TT_TABLE->first[i];
		__s64 prev_ida = -1;
		while (next) {
			if(next >= CURRENT_LAST) {
				if(!hashMap.grow())	{
					snprintf(buf, sizeof(buf), "** ERROR: out of range reference (i: %d, ref: %d, limit: %d)\n", i, next, CURRENT_LAST);
					*sbuf += buf;
					break;
				}
			}
			__s64 ida = TT_TABLE->elements[next].data.id.id;
			if (ida <= prev_ida) {
				snprintf(buf, sizeof(buf), "** ERROR: out of order elements found (i: %d, ida: %lld, prev_ida: %lld)\n", i, ida, prev_ida);
				*sbuf += buf;
				break;
			}

			int tcount = 0;
			int tnext = TT_TABLE->elements[next].data.id.next;
			while (tnext) {
				tcount++;
				tnext = TT_TABLE->elements[tnext].next;
			}
			if (!tcount) tzeros++;
			else {
				ttotal += tcount;
				if (tcount > tmax)
					tmax = tcount;
			}
			prev_ida = ida;
			next = TT_TABLE->elements[next].next;
			count++;
		}
		if (!count) zeros++;
		else {
			total += count;
			if (count > max) {
				max = count;
				idmax = i;
			}
		}
	}
	int next = TT_TABLE->elements[0].next;
	int hollows = 0, last = 0;
	while (next) {
		if (next >= CURRENT_LAST) {
			snprintf(buf, sizeof(buf), "** ERROR: hollow list out of range (%d)\n", next);
			*sbuf += buf;
			break;
		}
		hollows++;
		last = next;
		next = TT_TABLE->elements[next].next;
	}
	box_futex_unlock(&futex);
	snprintf(buf, sizeof(buf), "HASH_SIZE: %d, size: %d, mem: %p\n",
			HASH_SIZE, hashMap.get_size(), hashMap.get_mem()); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\thollows: %d\n", hollows); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\tlast:    %d\n", last); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\tttotal:  %d\n", ttotal); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\ttzeros:  %d\n", tzeros); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\tmax:     %d\n", tmax); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\ttmean:   %f\n", ttotal/(float)(total-tzeros)); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\ttotal:   %d\n", total); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\tzeros:   %d\n", zeros); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\tmean:    %f\n", total/(float)(HASH_SIZE-zeros)); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\tlost:    %d\n", last - total - hollows - ttotal); *sbuf += buf;
	snprintf(buf, sizeof(buf), "\tmax(%04d): %d\n", idmax, max); *sbuf += buf;
}

void Timetable::clean()
{
	init();
	box_futex_wlock(&futex);
	int fd = open(TIMETABLE_FNAME, O_RDWR);
	if (fd != -1) {
		void *zeros = calloc(4096, 1);
		int size = hashMap.get_size();
		while (size) {
			write (fd, zeros, 4096);
			size -= 4096;
		}
		free(zeros);
		close(fd);
	}
//try deleting...
//	unlink(PERMISSIONS_FNAME);
	box_futex_unlock(&futex);
	end();
}

void Timetable::dump_db(std::string *fname_buf)
{
	DLOG("Timetable::dump_db(%s)", fname_buf->c_str());
	int fd = -1;
	if(fname_buf->length())
	{
		if((fd = open(fname_buf->c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600)) == -1)
		{
			SELOG("Timetable::dump_db(%s) -> openning", fname_buf->c_str());
			return;
		}
	}
	char buf[256];
	box_futex_rlock(&futex);
// while(1) sleep(5);
	for(int i = 0; i < HASH_SIZE; i++)
	{
		std::string sbuf;
		int next = TT_TABLE->first[i];
		while (next) {
			perm_ID_t id = ID(TT_TABLE->elements[next].data.id.id, i);
			int perm_num = TT_TABLE->elements[next].data.id.id & 0x0ff; //max 20
			snprintf(buf, sizeof(buf), "%020llu|%02d{", id, perm_num); sbuf += buf;
			int tnext = TT_TABLE->elements[next].data.id.next;
			while(tnext)
			{
				tdata_t *td = (tdata_t *)TT_TABLE->elements[tnext].data.data;
				for(int i = 0; i < 3; i++)
				{
					if(td[i].mB == td[i].mE) break;
					{
						int d1 = td[i].mB / (60 * 24);
						int h1 = (td[i].mB - (d1*24*60)) / 60;
						int m1 = td[i].mB %  60;
						int d2 = td[i].mE / (60 * 24);
						int h2 = (td[i].mE - (d2*24*60)) / 60;
						int m2 = td[i].mE %  60;
						int n = snprintf(buf, sizeof(buf), "%d-%02d:%02d..%d-%02d:%02d,", d1, h1, m1, d2, h2, m2);
						sbuf.append(buf, n);
					}
				}
				tnext = TT_TABLE->elements[tnext].next;
			}
			next = TT_TABLE->elements[next].next;
			sbuf += "\b}\n";
		}
		if (fd != -1) {
			if (write(fd, sbuf.c_str(), sbuf.length()) != (int)sbuf.length()) {
				SELOG("Permission::dump_db(%s) -> writing", fname_buf->c_str());
			}
		} else {
			*fname_buf += sbuf;
		}
	}
	box_futex_unlock(&futex);

	if(fd != -1) close(fd);
}

