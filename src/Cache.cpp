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
      slab_(slab),
      readBuf_(),
      usedNodesLock_()
{
	// ensure hold a node(16K * 16 default).
	readBuf_.ensureWritableBytes(opts_.maxNodeMsg * opts_.maxNodeChildNum);
}

Cache::~Cache()
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
	
	MutexLockGuard lock(usedNodesLock_);
	
    if(newNode) {
        // get a new node
        char* p = (char*)slab_->alloc(sizeof(Node));
		n = new (p) Node(tree_, nid, slab_);
        usedNodes_.push_front(n);
	} else {
		NodeMap::iterator iter = nodes_.find(nid);
		if(iter == nodes_.end()) {
			LOGFMTI("cannot find node in memory.");
			// need to get node from disk
			readBuf_.retrieveAll();
			bool ret = layout_->find(nid, readBuf_);
			if(!ret) {
				LOGFMTA("cannot find error");
				return NULL;
			}
			char* p = (char*)slab_->alloc(sizeof(Node));
			n = new (p) Node(tree_, nid, slab_);

			n->deserialize(readBuf_);
			usedNodes_.push_front(n);
		} else {
			//LOGFMTI("find a node in memory.");
			usedNodes_.splice(usedNodes_.begin(), usedNodes_, nodes_[nid]);
		}
	}

	cacheSize_ += (*usedNodes_.begin())->writeBackSize();
	evictFromMemory();
	
    nodes_[nid] = usedNodes_.begin();
    return *usedNodes_.begin();
}
// 先读取赃的结点并写入磁盘
// 再根据结点的访问顺序，从内存中去除？
void Cache::writeBack()
{
    std::list<Node*>::iterator it, itEnd;
	
    std::map<nid_t, Node*> dirtyNodes;
    while(alive_) {

		// lock the usedNodes_ to find dirty nodes.
		{
		MutexLockGuard lock(usedNodesLock_);
		itEnd = usedNodes_.end();
        for(it = usedNodes_.begin(); it != itEnd; ++it) {
            if((*it)->dirty()) {
                dirtyNodes[(*it)->nid()] = *it;
            }
        }
		}
		
		LOGFMTI("Cache::writeBack flush nodes [%lu]", dirtyNodes.size());
		
        if(dirtyNodes.size())
            flushDirtyNodes(dirtyNodes);

        ::usleep(1000 * 100); // 100ms
    }
}

void Cache::flushDirtyNodes(std::map<nid_t, Node*>& dirtyNodes)
{
    layout_->write(dirtyNodes);

	std::map<nid_t, Node*>::iterator it, itEnd = dirtyNodes.end();
	for(it = dirtyNodes.begin(); it != itEnd; ++it) {
		Node* node = it->second;
		node->setDirty(false);
        node->setFlushing(false);
	}
}

void Cache::evictFromMemory()
{
	size_t limitedMem = opts_.cacheLimitMem; // default 256M
	Node* node = NULL;
	
	std::list<Node*>::reverse_iterator it = usedNodes_.rbegin(), itEnd = usedNodes_.rend();
	std::list<Node*>::iterator iter;
	
	while(cacheSize_ >= limitedMem && it != itEnd) {
		node = *it;
		//node->writeLock();
		if(node->dirty()) {
			nodes_.erase(node->nid());
			cacheSize_ -= node->writeBackSize();
			iter = usedNodes_.erase((++it).base());
			it = std::list<Node*>::reverse_iterator(iter);
		} else
			++it;
		//node->writeUnlock();
	}	
}

void Cache::flush()
{
	;
}

