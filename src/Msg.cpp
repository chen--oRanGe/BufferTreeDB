#include "Msg.h"
#include "Slab.h"
#include "Mutex.h"

using namespace bt;

MsgBuf::MsgBuf(Comparator* cmp, Slab* slab)
    : list_(Compare(cmp), slab),
      slab_(slab),
      cmp_(cmp),
      mutex_(),
      size_(0)
{
}

MsgBuf::~MsgBuf()
{
    Iterator iter(&list_);
    iter.seekToFirst();

    while(iter.valid()) {
        Msg msg = iter.key();
        msg.release();
        iter.next();
    }

    list_.clear();
}

size_t MsgBuf::count()
{
    return list_.count();
}

size_t MsgBuf::size()
{
    return 4 + size_;
}

size_t MsgBuf::memUsage()
{
    return list_.memUsage() + sizeof(MsgBuf);
}

void MsgBuf::clear()
{
    assert(mutex_.isLockedByThisThread());

    list_.clear();
    size_ = 0;
}

void MsgBuf::insert(const Msg& msg)
{
    assert(mutex_.isLockedByThisThread());

    Iterator iter(&list_);
    Msg got;
    bool release = false;
    iter.seek(msg);
    if(iter.valid()) {
        got = iter.key();

        if(got.key() == msg.key()) {
            size_ -= got.size();
            release = true;
        }
    }
    list_.insert(msg);
    size_ += msg.size();
    if(release)
        got.release();
}

void MsgBuf::resize(size_t size)
{
    assert(mutex_.isLockedByThisThread());

    list_.resize(size);

    size_ = 0;
    Iterator iter(&list_);
    iter.seekToFirst();

    while(iter.valid()) {
        size_ += iter.key().size();
        iter.next();
    }
}

bool MsgBuf::find(Slice key, Msg& msg)
{
    assert(mutex_.isLockedByThisThread());

	Slice value = Slice();
    Msg fake(Nop, key, value);
    Iterator iter(&list_);

    iter.seek(fake);

    if(iter.valid() && iter.key().key() == key) {
        msg = iter.key();
        return true;
    }

    return false;
}

bool MsgBuf::deserialize(Buffer& reader)
{
    MutexLockGuard lock(mutex_);
    uint32_t count = reader.readInt32();

    if(count == 0)
        return true;

    for(size_t i = 0; i < count; ++i) {
        uint8_t type;
        Slice value;
		type = reader.readInt8();
		std::string keyStr(reader.readString());
		Slice key(keyStr);
        if(type == Put) {
			std::string valueStr(reader.readString());
            value = Slice(valueStr);
        }

        Msg msg((MsgType)type, key, value);
        list_.insert(msg);
        size_ += msg.size();
    }

    return true;
}

bool MsgBuf::serialize(Buffer& writer)
{
    MutexLockGuard lock(mutex_);

	int count = list_.count();
	writer.appendInt32(list_.count());

    Iterator iter(&list_);
    iter.seekToFirst();

    while(iter.valid()) {
        Msg msg = iter.key();
        uint8_t type = msg.type();
		
		writer.appendInt8(type);
		writer.append(msg.key().data(), msg.key().size());

        if(type == Put)
			writer.append(msg.value().data(), msg.value().size());

        count--;
        iter.next();
    }
    assert(count == 0);
    return true;
}
