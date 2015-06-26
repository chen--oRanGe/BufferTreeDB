#include "Logger.h"
#include "Node.h"
#include "BufferTree.h"
#include "Mutex.h"

using namespace bt;

Node::Node(BufferTree* tree, nid_t self, Slab* slab)
    : tree_(tree),
      self_(self),
      refcnt_(0),
      dirty_(false),
      flushing_(false),
      slab_(slab)
{}

Node::~Node()
{}

void Node::createFirstPivot()
{
    writeLock();
    
    assert(pivots_.size() == 0);
	Slice empty = Slice();
    addPivot(NID_NIL, NULL, empty);

    writeUnlock();
}

bool Node::get(const Slice& key, Slice& value, Node* parent)
{
    readLock();
    if(parent) {
        parent->readUnlock();
    }

    size_t index = findPivot(key);
    MsgBuf* buf = pivots_[index].buf;

    buf->lock();

    Msg lookup;
    if(buf->find(key, lookup) && lookup.key() == key) {
        if(lookup.type() == Put) {
            value = lookup.value().clone(slab_);
            buf->unlock();
            readUnlock();
            return true;
        } else {
            buf->unlock();
            readUnlock();
            return false;
        }
    }
    buf->unlock();

    if(pivots_[index].childNid == NID_NIL) {
        readUnlock();
        return false;
    }

    Node* node = tree_->getNode(pivots_[index].childNid);
    assert(node);

    bool exists = node->get(key, value, this);
    //node->decRef();

    return exists;
}

bool Node::put(const Slice& key, const Slice& value)
{
    return write(Msg(Put, key.clone(slab_), value.clone(slab_)));
}

bool Node::del(const Slice& key)
{
    return write(Msg(Del, key.clone(slab_)));
}

bool Node::write(const Msg& msg)
{
    assert(pivots_.size());
    optionalLock();

    // must insert from root node.
    if(tree_->root_->nid() != self_) {
        optionalUnlock();
        return tree_->root_->write(msg);
    }

	size_t idx = findPivot(msg.key());
	LOGFMTA("Node::write findPivot indx [%lu]", idx);
    insertMsg(idx, msg);
    setDirty(true);

    pushDownOrSplit();
    return true;
}

void Node::pushDownOrSplit()
{
    int index = -1;
    // find which Pivot need to split
    for(size_t i = 0; i < pivots_.size(); ++i) {
		// > 16*1024
		LOGFMTI("Node::pushDownOrSplit [%lu,     %lu]", i, pivots_[i].buf->size());
        if(pivots_[i].buf->size() > tree_->opts_.maxNodeMsg) {
            index = i;
            break;
        }
    }
    if(index < 0) {
        optionalUnlock();
        return;
    }
    if(pivots_[index].childNid != NID_NIL) {
        // if have child, flush data to the childs.
        MsgBuf* buf = pivots_[index].buf;
        Node* node = tree_->getNode(pivots_[index].childNid);
        node->pushDown(buf, this);
        //node->decRef();
    } else {
        // if no child, split the Pivot.
        splitBuf(pivots_[index].buf);
    }

    optionalLock();
    pushDownOrSplit();
}

void Node::insertMsg(size_t index, const Msg& msg)
{
    MsgBuf* buf = pivots_[index].buf;

    buf->lock();
    buf->insert(msg);
    buf->unlock();
}

void Node::splitBuf(MsgBuf* buf)
{
    assert(isLeaf_);

    // if pivot bigger than 16K
    if(buf->size() <= tree_->opts_.maxNodeMsg) {
        writeUnlock();
        return;
    }

	LOGFMTI("############# Node::splitBuf ############ ");

    MsgBuf* buf0 = buf;
    MsgBuf* buf1 = new MsgBuf(slab_);

    buf0->lock();

    MsgBuf::Iterator iter(buf0->skiplist());

    iter.seekToFirst();
    assert(iter.valid());
    Msg msg = iter.key();

    iter.seekToMiddle();
    assert(iter.valid());
    Msg middle = iter.key();

    buf1->lock();
    while(iter.valid()) {
        buf1->insert(iter.key());
        iter.next();
    }

    size_t sz = buf0->size();
    buf0->resize(buf0->count() / 2);

    addPivot(NID_NIL, buf1, middle.key().clone(slab_));

    assert(buf0->size() + buf1->size() >= sz);

    buf1->unlock();
    buf0->unlock();

    setDirty(true);
    writeUnlock();

	// lock the nodes from root to leaf.
    std::vector<Node*> lockedPath;
    tree_->lockPath(msg.key(), lockedPath);

	LOGFMTI("Node::splitBuf lockedPath size: %lu", lockedPath.size());

    if(!lockedPath.empty()) {
        Node* node = lockedPath.back();
        node->splitNode(lockedPath);
    }
}

void Node::addPivot(nid_t child, MsgBuf* buf, Slice key)
{
    MutexLockGuard lock(pivotsMutex_);
    if(key.size() == 0) {
        assert(buf == NULL);
        assert(pivots_.size() == 0);

        buf = new MsgBuf(slab_);
        pivots_.push_back(Pivot(child, buf, key));
    } else {
        assert(pivots_.size());
        if(buf == NULL) {
            buf = new MsgBuf(slab_);
        }
        size_t idx = findPivot(key);
		LOGFMTI("Node::addPivot findPivot indx [%lu]", idx);
        // FIXME
        pivots_.insert(pivots_.begin() + idx + 1, Pivot(child, buf, key));
    }

    setDirty(true);
}

size_t Node::findPivot(const Slice& key)
{
	size_t pivots = pivots_.size();
	if(pivots == 0)
		return 0;
	size_t pivot = 0;

	for(size_t i = 0; i < pivots; i++) {
		LOGFMTA("Node::findPivot indx [%s,    %s]", std::string(key.data(), key.size()).c_str(), 
			std::string(pivots_[i].leftKey.data(), pivots_[i].leftKey.size()).c_str());
		if(key.compare(pivots_[i].leftKey) < 0)
			return pivot;
		pivot++;
	}
	if(pivot > (pivots - 1))
		return pivots - 1;
	return pivot;
}

void Node::lockPath(const Slice& key, std::vector<Node*>& path)
{
    path.push_back(this);

    size_t index = findPivot(key);
	LOGFMTI("Node::lockPath findPivot index [%lu]", index);

    if(pivots_[index].childNid != NID_NIL) {
        Node* node = tree_->getNode(pivots_[index].childNid);

        node->writeLock();
        node->pushDownLocked(pivots_[index].buf, this);
        node->lockPath(key, path);
    }
}

void Node::pushDown(MsgBuf* buf, Node* parent)
{
    optionalLock();

    pushDownLocked(buf, parent);
    parent->readUnlock();

    pushDownOrSplit();
}


void Node::pushDownLocked(MsgBuf* buf, Node* parent)
{
    buf->lock();

    if(buf->count() == 0) {
        buf->unlock();
        return;
    }

    size_t idx = 1;
    size_t i = 0, j = 0;
    MsgBuf::Iterator slow(buf->skiplist());
    MsgBuf::Iterator fast(buf->skiplist());

    slow.seekToFirst();
    fast.seekToFirst();

    while(fast.valid() && idx < pivots_.size()) {
		if(fast.key().key().compare(pivots_[idx].leftKey) < 0) {
            j++;
            fast.next();
        } else {
            while(i != j) {
                insertMsg(idx - 1, slow.key());
                i++;
                slow.next();
            }
            idx++;
        }
    }

    while(slow.valid()) {
        insertMsg(idx - 1, slow.key());
        slow.next();
    }

    setDirty(true);
    parent->setDirty(true);
    buf->clear();
    buf->unlock();
}

void Node::splitNode(std::vector<Node*>& path)
{
    assert(path.back() == this);

	// <= 16
    if(pivots_.size() <= tree_->opts_.maxNodeChildNum) {
        while(!path.empty()) {
            Node* node = path.back();
            node->writeUnlock();
            //node->decRef();
            path.pop_back();
        }
        return;
    }

	LOGFMTI("############# Node::splitNode ############ ");
	
    size_t middle = pivots_.size() / 2;
    Slice middleKey = pivots_[middle].leftKey;

    Node* node = tree_->createNode();
    node->isLeaf_ = isLeaf_;

    std::vector<Pivot>::iterator first = pivots_.begin() + middle;
    std::vector<Pivot>::iterator last = pivots_.end();

    node->pivots_.insert(node->pivots_.begin(), first, last);
    node->setDirty(true);
    //node->decRef();

    pivots_.resize(middle);
    setDirty(true);
    path.pop_back();

    if(path.empty()) {
        Node* root = tree_->createNode();
        root->isLeaf_ = false;
        root->addPivot(nid(), NULL, Slice());
        root->addPivot(node->nid(), NULL, middleKey.clone(slab_));
        tree_->growUp(root);
    } else {
        Node* parent = path.back();
        parent->addPivot(node->nid(), NULL, middleKey.clone(slab_));
        parent->splitNode(path);
    }

    writeUnlock();
    //decRef();
}

size_t Node::size()
{
    MutexLockGuard lock(pivotsMutex_);
    //size_t usage = sizeof(Node);
    size_t usage = 0;
    for(size_t i = 0; i < pivots_.size(); ++i)
        usage += pivots_[i].buf->size();

    //return usage + pivots_.size() * sizeof(Pivot);
    return usage;
}

void Node::setDirty(bool dirty)
{
    MutexLockGuard lock(mutex_);

    //if (!dirty_ && dirty) 
        //first_write_timestamp_ = Timestamp::now();

    dirty_ = dirty;
}

bool Node::dirty() 
{
    MutexLockGuard lock(mutex_);
    return dirty_;
}

void Node::setFlushing(bool flushing)
{
    MutexLockGuard lock(mutex_);
    flushing_ = flushing;
}

size_t Node::writeBackSize()
{
    size_t size = 0;

    size += sizeof(isLeaf_);
    size += sizeof(self_);
    size += 4; // pivots_.size()

    for(size_t i = 0; i < pivots_.size(); ++i) {
        size += sizeof(nid_t); // childNid
        size += 4 + pivots_[i].leftKey.size(); // leftKey
        size += pivots_[i].buf->size(); // buf size
    }

    return size;
}

bool Node::flushing()
{
    MutexLockGuard lock(mutex_);
    return flushing_;
}

void Node::incRef()
{
    MutexLockGuard lock(mutex_);
    refcnt_++;
}

void Node::decRef()
{
    MutexLockGuard lock(mutex_);
    assert(refcnt_ > 0);

    refcnt_--;
    //last_used_timestamp_ = Timestamp::now();
}

size_t Node::refs()
{
    MutexLockGuard lock(mutex_);
    return refcnt_;
}

nid_t Node::nid() 
{
    MutexLockGuard lock(mutex_);
    return self_;
}

void Node::setNid(nid_t nid)
{
    MutexLockGuard lock(mutex_);
    self_ = nid;
}

void Node::setLeaf(bool leaf)
{
    MutexLockGuard lock(mutex_);
    isLeaf_ = leaf;
}

bool Node::serialize(Buffer& writer)
{
	writer.appendInt8(isLeaf_);
	writer.appendInt32(self_);
	uint32_t pivots = pivots_.size();
	assert(pivots > 0);
	writer.appendInt32(pivots);

	for(size_t i = 0; i < pivots; ++i) {
		writer.appendInt32(pivots_[i].childNid);
		writer.appendInt32(pivots_[i].leftKey.size());
		writer.append(pivots_[i].leftKey.data(), pivots_[i].leftKey.size());
		pivots_[i].buf->serialize(writer);
	}

	return true;
}

bool Node::deserialize(Buffer& reader)
{
	isLeaf_ = reader.readInt8();
	self_ = reader.readInt32();
	
	uint32_t pivots = 0;
	pivots = reader.readInt32();
	assert(pivots > 0);

	nid_t child = 0;
	//Slice leftKey;
	MsgBuf* buf = NULL;
	for(size_t i = 0; i < pivots; ++i) {
		child = reader.readInt32();
		std::string readStr(reader.readString());
		Slice leftKey(readStr);
		buf = new MsgBuf(slab_);
		buf->deserialize(reader);
		pivots_.push_back(Pivot(child, buf, leftKey));
	}
	setDirty(true);
	return true;
}

