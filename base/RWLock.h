#ifndef __BT_RWLOCK_H
#define __BT_RWLOCK_H

#include <boost/noncopyable.hpp>
#include "Mutex.h"
#include "Condition.h"

namespace bt {
class RWLock : boost::noncopyable
{
public:
    RWLock();
	~RWLock();

    bool tryReadLock();
    void readLock();

    bool tryWriteLock();
    void writeLock();
    void unlock();
private:
	RWLock(const RWLock&);
    RWLock& operator =(const RWLock&);
	pthread_rwlock_t rwlock_;
};
}

#endif
