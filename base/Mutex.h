#ifndef __BT_MUTEX_H
#define __BT_MUTEX_H
#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>
#include "Thread.h"

/*
#ifdef CHECK_PTHREAD_RETURN_VALUE

#ifdef NDEBUG
    __BEGIN_DECLS
extern void __assert_perror_fail (int errnum,
        const char *file,
        unsigned int line,
        const char *function)
__THROW __attribute__ ((__noreturn__));
__END_DECLS
#endif

#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
        if (__builtin_expect(errnum != 0, 0))    \
        __assert_perror_fail (errnum, __FILE__, __LINE__, __func__);})

#else  // CHECK_PTHREAD_RETURN_VALUE

#define MCHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
        assert(errnum == 0); (void) errnum;})

#endif // CHECK_PTHREAD_RETURN_VALUE
*/

#define MCHECK(ret) ret

namespace bt
{

    // Use as data member of a class, eg.
    //
    // class Foo
    // {
    //  public:
    //   int size() const;
    //
    //  private:
    //   mutable MutexLock mutex_;
    //   std::vector<int> data_; // GUARDED BY mutex_
    // };
class MutexLock : boost::noncopyable
{
public:
    MutexLock()
        : holder_(0)
    {
        MCHECK(pthread_mutex_init(&mutex_, NULL));
    }

    ~MutexLock()
    {
        assert(holder_ == 0);
        MCHECK(pthread_mutex_destroy(&mutex_));
    }

    // must be called when locked, i.e. for assertion
    bool isLockedByThisThread() const
    {
        return holder_ == currentData::getTid();
    }

    void assertLocked() const
    {
        assert(isLockedByThisThread());
    }

    // internal usage

    void lock()
    {
        MCHECK(pthread_mutex_lock(&mutex_));
        holder_ = currentData::getTid();
    }

    void unlock()
    {
        holder_ = 0;
        MCHECK(pthread_mutex_unlock(&mutex_));
    }

    pthread_mutex_t* getPthreadMutex() /* non-const */
    {
        return &mutex_;
    }

private:
    pid_t holder_;
    pthread_mutex_t mutex_;
};

    // Use as a stack variable, eg.
    // int Foo::size() const
    // {
    //   MutexLockGuard lock(mutex_);
    //   return data_.size();
    // }
class MutexLockGuard : boost::noncopyable
{
public:
    explicit MutexLockGuard(MutexLock& mutex)
        : mutex_(mutex)
    {
        mutex_.lock();
    }

    ~MutexLockGuard()
    {
        mutex_.unlock();
    }

private:

    MutexLock& mutex_;
};

}

#endif
