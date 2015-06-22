#include <boost/bind.hpp>
#include <assert.h>

#include "Cache.h"
#include "Thread.h"
#include "Logger.h"
#include "Node.h"
#include "Layout.h"

using namespace bt;

Cache::Cache(const Options& opts, Slab* slab)
    : opts_(opts),
      cacheSize_(0),
      alive_(false),
      worker_(NULL),
      slab_(slab)
{}

bool Cache::init()
{
    alive_ = true;
    worker_ = new Thread(boost::bind(&Cache::writeBack, this));

    if(worker_ == NULL) {
        LOGFMTA("Cache::init error");
        return false;
    }

    worker_->start();
    return true;
}

void Cache::tie(BufferTree* tree, Layout* layout)
{
    assert(tree && layout);

    tree_ = tree;
    layout_ = layout;

    //lastCheckpoint = Timestamp::now();
}

Node* Cache::getNode(nid_t nid, bool newNode)
{
    Node* n = NULL;
    if(newNode) {
        // get a new node
        char* p = (char*)slab_->alloc(sizeof(Node));
		n = new (p) Node(tree_, nid, slab_);
        usedNodes_.push_front(n);
        nodes_[nid] = usedNodes_.begin();
        return n;
    }

    NodeMap::iterator iter = nodes_.find(nid);
    if(iter == nodes_.end()) {
		LOGFMTI("cannot find node in memory.");
		// need to get node from disk
		Buffer buf;
        bool ret = layout_->find(nid, buf);
        if(!ret) {
            LOGFMTA("cannot find error");
			return NULL;
        }
		char* p = (char*)slab_->alloc(sizeof(Node));
		n = new (p) Node(tree_, nid, slab_);
		n->deserialize(buf);
        usedNodes_.push_front(n);
    } else {
		LOGFMTI("find a node in memory.");
        usedNodes_.splice(usedNodes_.begin(), usedNodes_, nodes_[nid]);
    }
    nodes_[nid] = usedNodes_.begin();
    return *usedNodes_.begin();
}
// 先读取赃的结点并写入磁盘
// 再根据结点的访问顺序，从内存中去除？
void Cache::writeBack()
{
    size_t limitedMem = opts_.cacheLimitMem; // default 256M
    size_t num = 0;
    std::list<Node*>::iterator iter, it;
    std::vector<Node*> dirtyNodes;
    while(alive_) {
        for(iter = usedNodes_.begin(); iter != usedNodes_.end(); ++iter) {
            if((*iter)->dirty()) {
                dirtyNodes.push_back((*iter));
            }
            if((cacheSize_ < limitedMem) && ((cacheSize_ + (*iter)->size()) < limitedMem)) {
                cacheSize_ += (*iter)->size();
                num++;
            }
        }
		
		LOGFMTI("Cache::writeBack cache nodes [%d]", num);
		LOGFMTI("Cache::writeBack flush nodes [%d]", dirtyNodes.size());
		
        if(dirtyNodes.size())
            flushDirtyNodes(dirtyNodes);
        int leftNodes = usedNodes_.size() - num;
        for(int i = 0; i < leftNodes; i++) {
			cacheSize_ -= usedNodes_.back()->size();
            nodes_.erase(usedNodes_.back()->nid());
            usedNodes_.pop_back();
        }
        ::usleep(1000 * 100); // 100ms
    }
}

void Cache::flushDirtyNodes(std::vector<Node*>& dirtyNodes)
{
    layout_->write(dirtyNodes);
	
	size_t size = dirtyNodes.size();
    for(size_t i = 0; i < size; ++i)
    {
        Node* node = dirtyNodes[i];
        node->setDirty(false);
        node->setFlushing(false);
    }
}

void Cache::flush()
{
	;
}

