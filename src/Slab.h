#ifndef __BT_SLAB_H
#define __BT_SLAB_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

namespace bt {

struct Page
{
    uintptr_t slab;
    Page* next;
    uintptr_t prev;
    Page()
      : slab(0), next(0), prev(0)
    {}
};

class Slab
{
public:
    Slab();
    ~Slab();
    bool init(uint32_t slabSize);
	bool clear();
    void* alloc(uint32_t size);
    void* allocLocked(uint32_t size);
	Page* allocPages(uint32_t pages);
    void free(void* p);
    void freeLocked(void* p);
	void freePages(Page* page, uint32_t pages);
    void slabStat();
private:
    //struct Stat stat_;

    uint32_t minSize_;
    uint32_t minShift_;
    Page* pages_;
    Page* last_;
    Page free_;

    char* start_;
    char* end_;

    char* addr_;

    uint32_t maxSize_;
    uint32_t exactSize_;
    uint32_t exactShift_;
    uint32_t pageSize_;
    uint32_t pageShift_;
    uint32_t realPages_;
};
}

#endif
