#ifndef __BT_DB_H
#define __BT_DB_H

#include "Slice.h"
#include "Options.h"
#include "Comparator.h"

namespace bt {
class DB {
public:
    static DB* open(const std::string& dbname, const Options& opts);
	virtual ~DB(){}
    virtual bool put(Slice& key, Slice& value) = 0;
    virtual bool get(Slice& key, Slice& value) = 0;
    virtual bool del(Slice& key) = 0;
};
}

#endif
