#ifndef __COMPARATOR_H
#define __COMPARATOR_H

#include "Slice.h"
namespace bt {
class Comparator {
public:
    virtual int compare(const Slice& a, const Slice& b) const = 0;
};

class BytewiseComparator : public Comparator {
public:
    int compare(const Slice& a, const Slice& b) const
    {
        return a.compare(b);
    }
};
}

#endif
