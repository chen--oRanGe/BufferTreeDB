#ifndef __BT_BUFFERTREE_H
#define __BT_BUFFERTREE_H

#include <map>
#include <string>
#include <vector>

#include "Slice.h"
#include "Options.h"
#include "Mutex.h"

namespace bt
{
class Node;
class Layout;
class Cache;

class BufferTree
{
public:
    BufferTree(const std::string& name, Options& opts, Cache* cache, Layout* layout);
    ~BufferTree();

    bool init();
    void growUp(Node* root);
    bool put(const Slice& key, const Slice& value);
    bool del(const Slice& key);
    bool get(const Slice& key, Slice& value);

    Node* createNode(nid_t nid);
	Node* createNode();
    Node* getNode(nid_t nid);
    void lockPath(const Slice& key, std::vector<Node*>& path);
private:
    friend class Node;
    std::string name_;
    Options opts_;
    Cache* cache_;
    Node* root_;
    nid_t nodeCount_;
    std::map<nid_t, Node*> nodeMap_;
    MutexLock mutex_;
    MutexLock mutexLockPath_;
	Layout* layout_;
};
}

#endif
