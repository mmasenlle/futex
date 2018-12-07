
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "logger.h"
#include "HashMap.h"

#define CHUNK_ADJUST ((16 * 1024) - 1)

HashMap::HashMap()
{
	mem = NULL;
	size = 0;
	fd = -1;
}

bool HashMap::open(const char *fname, int min_size)
{
	if((fd = ::open(fname, O_RDWR | O_CREAT, 0600)) == -1)
	{
		SELOG("HashMap::open() -> Cannot open '%s'", fname);
		return false;
	}
	struct stat sb;
	if(fstat(fd, &sb) == -1)
	{
		SELOG("HashMap::open() -> Cannot fstat '%s'", fname);
		::close(fd); fd = -1;
		return false;
	}
	size = sb.st_size;
	if(min_size > size)
	{
		size = (min_size + CHUNK_ADJUST) & ~(CHUNK_ADJUST);
		DLOG("HashMap::open() -> truncating to %d", size);
		if(ftruncate(fd, size) == -1)
		{
			SELOG("HashMap::open() -> Cannot ftruncate (%d)", size);
			::close(fd); fd = -1;
			size = 0;
			return false;
		}
	}
	mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	DLOG("HashMap::open() -> mem %p, size %d", mem, size);
	if(mem == MAP_FAILED)
	{
		SELOG("HashMap::open() -> Cannot mmap '%s'", fname);
		mem = NULL;
		size = 0;
		::close(fd); fd = -1;
		return false;
	}
	return true;
}

bool HashMap::grow(int min_size)
{
	DLOG("HashMap::grow(%d)", min_size);
	struct stat sb;
	if (fstat(fd, &sb) == -1) {
		SELOG("HashMap::grow() -> Cannot fstat");
		return false;
	}
	int new_size = sb.st_size;
	if (min_size > new_size) {
		new_size = (min_size + CHUNK_ADJUST) & ~(CHUNK_ADJUST);
		DLOG("HashMap::grow(%d) -> truncating to %d", min_size, new_size);
		if (ftruncate(fd, new_size) == -1) {
			SELOG("HashMap::grow() -> Cannot ftruncate (%d)", new_size);
			return false;
		}
	}
	void *m = mremap(mem, size, new_size, MREMAP_MAYMOVE);
	if (m == MAP_FAILED) {
		SELOG("HashMap::grow() -> Cannot mremap");
		return false;
	}
	mem = m;
	size = new_size;
	return true;
}

void HashMap::close()
{
	if (munmap(mem, size) == -1) {
		SELOG("HashMap::close() -> Cannot munmap");
	}
	if (::close(fd)) {
		SELOG("HashMap::close() -> Cannot close");
	}
	mem = NULL;
	size = 0;
	fd = -1;
}
