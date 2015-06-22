#ifndef __BT_CACHE_H
#define __BT_CACHE_H

#include <list>
#include <boost/unordered_map.hpp>

#include "Mutex.h"
#include "Options.h"
#include "RWLock.h"

namespace bt {

class BufferTree;
class Thread;
class Layout;
class Slab;
class Node;

struct CacheNode
{
    nid_t nid_;
    Node* node_;
    CacheNode(nid_t nid, Node* node)
        : nid_(nid), node_(node)
    {}
};

class Cache {
public:
    Cache(const Options& opts, Slab* slab);
    ~Cache();

    bool init();
    void tie(BufferTree* tree, Layout* layout);
    Node* getNode(nid_t nid, bool newNode);
    void flush();

	void writeBack();
	void flushDirtyNodes(std::vector<Node*>& dirtyNodes);

private:
    Options opts_;
    size_t cacheSize_;
    MutexLock mutex_;

    bool alive_;
    Thread* worker_;
    Layout* layout_;
    BufferTree* tree_;

    typedef boost::unordered_map<nid_t, std::list<Node*>::iterator> NodeMap;
    NodeMap nodes_;
    std::list<Node*> usedNodes_;
    RWLock lockNodes_;
	Slab* slab_;
};
}
#endif
