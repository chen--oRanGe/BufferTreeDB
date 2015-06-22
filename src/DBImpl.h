#ifndef __BT_DB_IMPL_H
#define __BT_DB_IMPL_H

#include "DB.h"

namespace bt {

class BufferTree;
class Cache;
class Layout;
class Slab;

class DBImpl : public DB
{
public:
    DBImpl(const std::string& name, const Options& opts)
        : name_(name),
          opts_(opts),
          layout_(NULL),
          cache_(NULL),
          bufferTree_(NULL),
          slab_(new Slab())
    {}
    ~DBImpl();

    bool init();
    bool put(Slice& key, Slice& value);
    bool get(Slice& key, Slice& value);
    bool del(Slice& key);

private:
    std::string name_;
    Options opts_;

    Layout* layout_;
    Cache* cache_;
    BufferTree* bufferTree_;
	Slab* slab_;
};

}

#endif
