#include "DBImpl.h"
#include "Logger.h"
#include "Layout.h"
#include "Cache.h"
#include "BufferTree.h"
#include "Slice.h"

using namespace bt;

DBImpl::~DBImpl()
{
    delete bufferTree_;
    delete cache_;
    delete layout_;
	delete slab_;
}

bool DBImpl::init()
{
	LOGFMTI("DBImpl::init SLAB_SIZE: %d", SLAB_SIZE);
	slab_ = new Slab();
	if(!slab_->init(SLAB_SIZE)) {// default 256M
		LOGFMTF("init slab error");
		return false;
	}
    layout_ = new Layout(name_);
    if(!layout_->init()) {
		LOGFMTF("init table error");
        return false;
    }

    cache_ = new Cache(opts_, slab_);
    if(!cache_->init()) {
		LOGFMTF("init cache error");
        return false;
    }

    bufferTree_ = new BufferTree(name_, opts_, cache_, layout_);
    if(!bufferTree_->init()) {
		LOGFMTF("init buffer tree error");
        return false;
    }

    return true;
}

DB* bt::DB::open(const std::string& name, const Options& opts)
{
    DBImpl* db = new DBImpl(name, opts);

    if(!db->init()) {
        delete db;
        return NULL;
    }

    return db;
}

bool DBImpl::put(Slice& key, Slice& value)
{
    return bufferTree_->put(key, value);
}

bool DBImpl::get(Slice& key, Slice& value)
{
    return bufferTree_->get(key, value);
}

bool DBImpl::del(Slice& key)
{
    return bufferTree_->del(key);
}
