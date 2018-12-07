#ifndef HASHMAP_H_
#define HASHMAP_H_

class HashMap
{
	void *mem;
	int size;
	int fd;
	
public:
	HashMap();
	bool open(const char *fname, int min_size);
	bool grow(int min_size = 0);
	inline void *get_mem() { return mem; };
	inline int get_size() { return size; };
	void close();
};

#endif /*HASHMAP_H_*/
