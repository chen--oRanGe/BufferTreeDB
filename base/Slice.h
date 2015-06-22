#ifndef __BT_SLICE_H
#define __BT_SLICE_H

#include <string.h>
#include <string>
#include <assert.h>
#include "Slab.h"

namespace bt {

class Slab;

class Slice {
public:
    Slice()
        : data_(NULL),
          size_(0),
          slab_(NULL)
    {}
    Slice(char* s)
        : data_(s),
          size_(strlen(s)),
          slab_(NULL)
    {}
    Slice(char* s, size_t size, Slab* slab = NULL)
        : data_(s),
          size_(size),
          slab_(slab)
    {}
    Slice(std::string& s)
        : data_(const_cast<char*>(s.data())),
          size_(s.size()),
          slab_(NULL)
    {}
	~Slice()
	{
	}
    char* data() const
    { return data_; }

    size_t size() const
    { return size_; }

    bool empty() const
    { return size_ == 0; }

    std::string toString() const
    { return std::string(data_, size_); }

    void clear()
    {
        data_ = NULL;
        size_ = 0;
    }

    char operator[] (size_t i) const
    {
        assert(i < size());
        return data_[i];
    }

    int compare(const Slice& slice) const
    {
        size_t prefixLen = (size_ < slice.size()) ? size_ : slice.size();
        int ret = memcmp(data_, slice.data(), prefixLen);
        if(ret == 0) {
            if(size_ < slice.size())
                ret = -1;
            else if(size_ > slice.size())
                ret = 1;
        }
        return ret;
    }

    Slice clone(Slab* slab) const
    {
        if(size_ == 0)
            return Slice();
		
        char* s = (char*)(slab->alloc(static_cast<uint32_t>(size_)));
        assert(s);
        memcpy(s, data_, size_);
        return Slice(s, size_, slab);
    }

	void release()
	{
		if(slab_ != NULL && data_ != NULL)
			slab_->free(data_);
	}
	
private:
    char* data_;
    size_t size_;
    Slab* slab_;
};

inline bool operator==(const Slice& x, const Slice& y)
{
    return x.compare(y) == 0;
}

inline bool operator!=(const Slice& x, const Slice& y)
{
    return !(x == y);
}

inline bool operator<(const Slice& x, const Slice& y)
{
    return x.compare(y) < 0;
}

}

#endif
