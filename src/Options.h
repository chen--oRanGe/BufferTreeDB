#ifndef __BT_OPRIONS_H
#define __BT_OPRIONS_H

namespace bt {

#define SLAB_SIZE 256 * 1024 * 1024 // 256MB
#define PAGE_SIZE (4*1024) // 4K

typedef uint32_t nid_t;
#define NID_NIL ((nid_t)0)


class Options
{
public:
    Options()
    {
        maxNodeChildNum = 4; // 16 nodes
        maxNodeMsg = 100; // 16K, 1 node at most 256K
        cacheLimitMem = 1 << 28; // 256M
        cacheDirtyNodeExpire = 1;
    }

    size_t maxNodeChildNum;
    size_t maxNodeMsg;
    size_t cacheLimitMem;
    size_t cacheDirtyNodeExpire;
};

}

#endif
