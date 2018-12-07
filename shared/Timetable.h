#ifndef TIMETABLE_H_
#define TIMETABLE_H_

#include <string>
#include <vector>
#include <set>
#include "HashMap.h"
#include "perm_ID_t.h"

class Timetable
{
	perm_ID_t id;
	int perm_num;
	struct tdata_t {
		unsigned short mB;
		unsigned short mE;
	};
 
	std::vector<tdata_t> vdata;
	std::set<unsigned long> *sdata;
	
	static HashMap hashMap;
	int cur, prev;
	bool locate();
	int _alloc();

	void clear_intervals(int first);
	int set_intervals();

	unsigned short convert(int wd, int h, int m);

public:
	void addInterval(int wdB, int wdE, int hB, int hE, int mB, int mE);
	void merge();
	bool match();
	void dump(std::string *buf);
	
	Timetable(perm_ID_t id, int pn, int lt = 0);
	~Timetable();
	bool commitIntervals();
	void put();
	void del();
	bool get();
	
	static void dump_db(std::string *fname_buf);
	static void stats(std::string *sbuf);
	static void clean();

	static void init();
	static void end();
};

#endif /*TIMETABLE_H_*/
