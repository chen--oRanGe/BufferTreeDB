#include "BufferTree.h"
#include "Cache.h"
#include "Layout.h"
#include "Node.h"
#include "Mutex.h"

using namespace bt;

BufferTree::BufferTree(const std::string& name, Options& opts,
        Cache* cache, Layout* layout)
    : name_(name),
      opts_(opts),
      cache_(cache),
      root_(NULL),
      nodeCount_(0),
      nodeMap_(),
      mutex_(),
      mutexLockPath_(),
      layout_(layout)
{}

bool BufferTree::init()
{
    cache_->tie(this, layout_);

    nid_t rootNid = layout_->getRootNid();
    nodeCount_ = layout_->getNodeCount();

    if(rootNid == NID_NIL) {
        assert(root_ == NULL);
        assert(nodeCount_ == 0);

		{
        MutexLockGuard lock(mutex_);
		++rootNid;
    	}
		
		root_ = createNode(rootNid);
        root_->setLeaf(true);
        root_->createFirstPivot();
    } else
		root_ = getNode(rootNid);

    return root_ != NULL;
}

void BufferTree::lockPath(const Slice& key, std::vector<Node*>& path)
{
    MutexLockGuard lock(mutexLockPath_);

    Node* root = root_;
    root->incRef();
    root->writeLock();

    if(root != root_) {
        root->writeUnlock();
        root->decRef();
    } else {
        root->lockPath(key, path);
    }
}

void BufferTree::growUp(Node* root)
{
    MutexLockGuard lock(mutex_);

    root_->decRef();
    root_ = root;
    layout_->setRootNid(root_->nid());
}

Node* BufferTree::createNode(nid_t nid)
{
    // 向Cache申请一个Node结点
	MutexLockGuard lock(mutex_);

	Node* node = cache_->getNode(nid, true);
    ++nodeCount_;

    return node;
}

Node* BufferTree::createNode()
{
	MutexLockGuard lock(mutex_);

	Node* node = cache_->getNode(nodeCount_, true);
    ++nodeCount_;

    return node;
}

Node* BufferTree::getNode(nid_t nid)
{
	MutexLockGuard lock(mutex_);
    return cache_->getNode(nid, false);
}

bool BufferTree::put(const Slice& key, const Slice& value)
{
    assert(root_);

    Node* root = root_;
    root_->incRef();
    bool succ = root->put(key, value);
    root->decRef();

    return succ;
}

bool BufferTree::del(const Slice& key)
{
    assert(root_);

    Node* root = root_;
    root->incRef();
    bool succ = root->del(key);
    root->decRef();

    return succ;
}

bool BufferTree::get(const Slice& key, Slice& value)
{
    assert(root_);

    Node* root = root_;
    root->incRef();
    bool succ = root->get(key, value);
    root->decRef();

    return succ;
}
