#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

#include "Options.h"
#include "Logger.h"
#include "DB.h"
#include "Slab.h"
#include "Skiplist.h"

using namespace bt;

static Slab* slab = NULL;

struct Cmp {
    int operator()(const int& a, const int& b) const 
    {
        if (a < b) return -1;
        else if (a > b) return 1;
        else return 0;
    }
};

void testEmpty()
{
    Cmp cmp;
    SkipList<int, Cmp> list(cmp, slab);

    assert(!list.contains(10));

    SkipList<int, Cmp>::Iterator iter(&list);

    assert(!iter.valid());
    iter.seekToFirst();
    assert(!iter.valid());
    iter.seekToMiddle();
    assert(!iter.valid());
    iter.seekToLast();
    assert(!iter.valid());
}

void testInsertErase()
{
    const size_t N = 100;
    Cmp cmp;
    SkipList<int, Cmp> list(cmp, slab);

    for (size_t i = 0; i < N; i += 2)
        list.insert(i);
    for (size_t i = 1; i < N; i += 2)
        list.insert(i);

    // test repeat
    for (size_t i = 0; i < N; i += 2)
        list.insert(i);
    for (size_t i = 1; i < N; i += 2)
        list.insert(i);

    assert(list.count() == N);

    SkipList<int, Cmp>::Iterator iter(&list);

    iter.seekToFirst();
    assert(cmp(iter.key(), 0) == 0);
    iter.seekToMiddle();
    assert(cmp(iter.key(), 50) == 0);

    
    list.erase(50);
    iter.seekToMiddle();
    assert(cmp(iter.key(), 49) == 0);
    
    list.insert(50);
    iter.seekToMiddle();
    assert(cmp(iter.key(), 50) == 0);


    std::vector<int> v1, v2;

    iter.seekToFirst();
    while (iter.valid()) {
        v1.push_back(iter.key());
        iter.next();
    }

    iter.seekToLast();
    while (iter.valid()) {
        v2.push_back(iter.key());
        iter.prev();
    }

    assert(v1.size() == v2.size());
    size_t size = v1.size();
    for (size_t i = 0; i < size; i++) {
        assert(v1[i] == v2[size-1-i]);
		LOGFMTI("v1 %lu: [%d,  %d]", i, v1[i], v2[i]);
    }

    // test resize
    
    LOGFMTI("before memory usage=%zu", list.memUsage());

    assert(list.count() == N); 
    list.resize(N / 2);

    LOGFMTI("after memory usage=%zu", list.memUsage());
    
    iter.seekToFirst();
    for (size_t i = 0; i < list.count(); i++) {
        assert(iter.key() == i);
        iter.next();
    }

}

int main()
{
    ILog4zManager::getRef().start();
    ILog4zManager::getRef().setLoggerLevel(LOG4Z_MAIN_LOGGER_ID,LOG_LEVEL_TRACE);


    LOGFMTT("Test slab begin...");

    slab = new Slab();
    slab->init(32 * 1024 * 1024); // 32M

/*
    char* p = NULL;
    for(int i = 0; i < 1000; i++) {
        p = (char*)slab->alloc(14 + i % 1024);
        if(p == NULL) {
            LOGFMTT("Alloc NULL potter [i: %d]", i);
            return -1;
        }
        strcpy(p, "hello");
        slab->free((void*)p);
    }
    slab->slabStat();
   
    LOGFMTT("Test slab end...");

    LOGFMTT("\nTest skiplist begin...");
*/	
    //testEmpty();
    testInsertErase();

    return 0;
}
