#ifndef __BT_NODE_H
#define __BT_NODE_H
#include <stdint.h>
#include <vector>

#include "RWLock.h"
#include "Slice.h"
#include "Msg.h"
#include "Options.h"

namespace bt {

class BufferTree;
class Slab;

struct Pivot
{
    Pivot() {}
    Pivot(nid_t _child, MsgBuf* _buf, Slice _key = Slice())
        : buf(_buf),
          childNid(_child),
          leftKey(_key)
    {}

    MsgBuf* buf;
    nid_t childNid;
    Slice leftKey;
};

class Node
{
public:
    Node(BufferTree* tree, nid_t self, Slab* slab);
    ~Node();


	void readLock()		{ rwlock_.readLock(); }
	void readUnlock()	{ rwlock_.unlock(); }

	void writeLock()		{ rwlock_.writeLock(); }
	void writeUnlock() 	{ rwlock_.unlock(); }

	bool tryReadLock()	{ return rwlock_.tryReadLock(); }
	bool tryWriteLock()	{ return rwlock_.tryWriteLock(); }

	void optionalLock()    { isLeaf_ ? writeLock() : readLock(); }
    void optionalUnlock()  { rwlock_.unlock(); }

	void createFirstPivot();
	bool get(const Slice& key, Slice& value, Node* parent = NULL);
	bool put(const Slice& key, const Slice& value);
	bool del(const Slice& key);
	bool write(const Msg& msg);
	void pushDownOrSplit();
	void insertMsg(size_t index, const Msg& msg);
	void splitBuf(MsgBuf* buf);
	void addPivot(nid_t child, MsgBuf* buf, Slice key);
	size_t findPivot(const Slice& key);
	void lockPath(const Slice& key, std::vector<Node*>& path);
	void pushDown(MsgBuf* buf, Node* parent);
	void pushDownLocked(MsgBuf* buf, Node* parent);
	void splitNode(std::vector<Node*>& path);
	size_t size();
	void setDirty(bool dirty);
	bool dirty();
	size_t writeBackSize();
	bool flushing();
	void setFlushing(bool flushing);
	void incRef();
	void decRef();
	size_t refs();
	nid_t nid();
	void setNid(nid_t nid);
	void setLeaf(bool leaf);
	bool serialize(Buffer& writer);
	bool deserialize(Buffer& reader);


private:
    BufferTree* tree_;
    nid_t self_;
    bool isLeaf_;
    size_t refcnt_;

    std::vector<Pivot> pivots_;
    MutexLock pivotsMutex_;
    RWLock rwlock_;
    MutexLock mutex_;
    bool dirty_;
    bool flushing_;
	Slab* slab_;
};
}

#endif
