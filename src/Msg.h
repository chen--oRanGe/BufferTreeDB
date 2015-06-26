#ifndef __BT_MSG_H
#define __BT_MSG_H

#include <vector>

#include "Slice.h"
#include "Skiplist.h"
#include "Mutex.h"
#include "Buffer.h"

namespace bt {
enum MsgType {
    Nop,
    Put,
    Del,
};

class Slab;

class Msg
{
public:
    Msg() : type_(Nop) {}
    Msg(MsgType type, Slice key, Slice value = Slice())
        : type_(type),
          key_(key),
          value_(value)
    {}
	~Msg()
	{}
    size_t size() const
    {
        size_t size = 0;
        size += 4;
        size += key_.size();
        if(type_ == Put)
            size += value_.size();
        return size;
    }

    void release()
    {
        key_.release();
        if(value_.size())
            value_.release();
    }

    Slice key() const { return key_; }
    Slice value() const { return value_; }
    MsgType type() const { return type_; }

private:
    MsgType type_;
    Slice key_;
    Slice value_;
};

class Compare
{
public:
    int operator()(const Msg& a, const Msg& b) const
    {
        return a.key().compare(b.key());
    }
};

class MsgBuf
{
public:
    typedef SkipList<Msg, Compare> List;
    typedef List::Iterator Iterator;

    MsgBuf(Slab* slab);
    ~MsgBuf();

    size_t count();
    size_t size();
    size_t memUsage();
    void clear();
    bool find(Slice key, Msg& msg);
    void insert(const Msg& msg);
    bool deserialize(Buffer& reader);
    bool serialize(Buffer& writer);

    void resize(size_t size);
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }

    List* skiplist() { return &list_; }
private:
	Slab* slab_;
    List list_;	
    MutexLock mutex_;
    size_t size_;
};
}

#endif
