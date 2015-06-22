#ifndef __BT_SKIPLIST_H
#define __BT_SKIPLIST_H

#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include <vector>
#include <boost/noncopyable.hpp>

namespace bt {

class Slab;

template<class Key, class Comparator>
class SkipList : boost::noncopyable 
{
private:
    struct Node;
public:
    explicit SkipList(Comparator cmp, Slab* slab);

    void insert(const Key& key);
    bool contains(const Key& key) const;
    void erase(const Key& key);
    void resize(size_t size);
    void clear();

    size_t count() const { return count_; }
    size_t memUsage() const 
	{ 
		//return slab_.usage(); 
		return 0;
	}

    class Iterator 
    {
        public:
            explicit Iterator(const SkipList* list);

            bool valid() const;
            const Key& key() const;
            void next();
            void prev();

            void seek(const Key& target);
            void seekToFirst();
            void seekToMiddle();
            void seekToLast();

        private:
            const SkipList* list_;
            Node* node_;
    };

private:
    enum { kMaxHeight = 17 };

    Slab* slab_;
    Node* head_;
    size_t maxHeight_;
    size_t count_;
    Comparator compare_;

    int seed_;

    int randomHeight();
    bool equal(const Key& a, const Key& b) const;

    Node* newNode(const Key& key, size_t height);
    Node* findGreaterOrEqual(const Key& key, Node** prev) const;
    Node* findLessThan(const Key& key) const;
};

template<class Key, class Comparator>
struct SkipList<Key, Comparator>::Node 
{
    explicit Node(const Key& k) : key(k) { } 
    Key key;
    Node* next(size_t n) { return next_[n]; }
    void setNext(size_t n, Node* node) { next_[n] = node; }
    void setKey(const Key& k) { key = k; }

private:
    Node* next_[1];
};

template<class Key, class Comparator>
typename SkipList<Key, Comparator>::Node* 
SkipList<Key, Comparator>::newNode(const Key& key, size_t height)
{
    size_t size = sizeof(Node) + sizeof(Node*) * (height - 1);
    char* allocPtr = (char*)slab_->alloc(size);

    return new (allocPtr) Node(key);
}

template<class Key, class Comparator>
inline SkipList<Key, Comparator>::Iterator::Iterator(const SkipList* list)
{
    list_ = list;
    node_ = NULL;
}

template<class Key, class Comparator>
inline bool SkipList<Key, Comparator>::Iterator::valid() const 
{
    return node_ != NULL;
}

template<class Key, class Comparator>
inline const Key& SkipList<Key, Comparator>::Iterator::key() const
{
    assert(valid());
    return node_->key;
}

template<class Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::next()
{
    assert(valid());
    node_ = node_->next(0);
}

template<class Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::prev()
{
    assert(valid());

    node_ = list_->findLessThan(node_->key);
    if (node_ == list_->head_)
        node_ = NULL;
}

template<class Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::seek(const Key& target)
{
    node_ = list_->findGreaterOrEqual(target, NULL);
    if (node_ == list_->head_)
        node_ = NULL;
}

template<class Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::seekToFirst()
{
    node_ = list_->head_->next(0);
}

template<class Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::seekToMiddle()
{
    int middle = list_->count_ / 2; 

    seekToFirst();

    for (int i = 0; i < middle; i++)
        node_ = node_->next(0);
}

template<class Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::seekToLast()
{
    Node* curr = list_->head_;
    size_t level = list_->max_height_ - 1;

    while (true) {
        Node* next = curr->next(level);

        if (next == NULL) {
            if (level == 0)
                break;
            else 
                level--;
        } else {
            curr = next;
        }
    }

    node_ = curr;
    if (node_ == list_->head_)
        node_ = NULL; 
}

template<class Key, class Comparator>
bool SkipList<Key, Comparator>::equal(const Key& a, const Key& b) const
{
    return compare_(a, b) == 0;
}

template<class Key, class Comparator>
int SkipList<Key, Comparator>::randomHeight()
{
    static const size_t kBranching = 4;
    int height = 1;

    while (height < kMaxHeight && (rand() % kBranching) == 0)
        height++;

    return height;
}

template<class Key, class Comparator>
typename SkipList<Key, Comparator>::Node* 
SkipList<Key, Comparator>::findGreaterOrEqual(const Key& key, Node** prev) const
{
    Node* curr = head_;
    size_t level = maxHeight_ - 1;

    while (true) {
        Node* next = curr->next(level);

        if (next != NULL && compare_(next->key, key) < 0) {
            curr = next;
        } else {
            if (prev != NULL)
                prev[level] = curr;

            if (level == 0)
                return next;
            else
                level--;
        }
    } 
}

template<class Key, class Comparator>
typename SkipList<Key, Comparator>::Node* 
SkipList<Key, Comparator>::findLessThan(const Key& key) const
{
    Node* curr = head_;
    size_t level = maxHeight_ - 1;

    while (true) {
        Node* next = curr->next(level);

        if (next == NULL || compare_(next->key, key) >= 0) {
            if (level == 0)
                return curr;
            else
                level--;
        } else {
            curr = next;
        }
    }
}

template<class Key, class Comparator>
SkipList<Key, Comparator>::SkipList(Comparator cmp, Slab* slab)
        : slab_(slab), head_(newNode(Key(), kMaxHeight)),
          maxHeight_(1), count_(0), 
          compare_(cmp), seed_(time(NULL))
{
    srand(seed_);

    for (int i = 0; i < kMaxHeight; i++)
        head_->setNext(i, NULL);
}

template<class Key, class Comparator>
void SkipList<Key, Comparator>::insert(const Key& key)
{
    Node* prev[kMaxHeight];
    Node* next = findGreaterOrEqual(key, prev);

    size_t height = randomHeight();

    if (height > maxHeight_) {
        for (size_t i = maxHeight_; i < height; i++)
            prev[i] = head_;

        maxHeight_ = height;
    }

    if (next && equal(next->key, key)) {
        next->setKey(key);
    } else {
        Node* curr = newNode(key, height);

        for (size_t i = 0; i < height; i++) {
            curr->setNext(i, prev[i]->next(i));
            prev[i]->setNext(i, curr);
        }

        count_++;
    }
}

template<class Key, class Comparator>
bool SkipList<Key, Comparator>::contains(const Key& key) const
{
    Node* x = findGreaterOrEqual(key, NULL);

    if (x != NULL && equal(x->key, key))
        return true;
    else
        return false;
}

template<class Key, class Comparator>
void SkipList<Key, Comparator>::erase(const Key& key)
{
    Node* prev[kMaxHeight];
    Node* curr = findGreaterOrEqual(key, prev);

    assert(curr != NULL);
    assert(equal(curr->key, key));

    for (size_t i = 0; i < maxHeight_; i++) {
        if (prev[i]->next(i) == curr)
            prev[i]->setNext(i, curr->next(i));
    }

    count_--;
}

template<class Key, class Comparator>
void SkipList<Key, Comparator>::resize(size_t size)
{
    assert(size <= count_);

    std::vector<Key> keys;
    keys.reserve(size);

    Iterator iter(this);
    iter.seekToFirst();

    for (size_t i = 0; i < size; i++) {
        assert(iter.valid());
        keys.push_back(iter.key());
        iter.next();
    }

    clear();

    for (size_t i = 0; i < keys.size(); i++) 
        insert(keys[i]);

    count_ = size;
}

template<class Key, class Comparator>
void SkipList<Key, Comparator>::clear()
{
    //slab_.clear();
    head_ = newNode(Key(), kMaxHeight);

    for (int i = 0; i < kMaxHeight; i++)
        head_->setNext(i, NULL);

    count_ = 0;
    maxHeight_ = 1;
}

}

#endif
