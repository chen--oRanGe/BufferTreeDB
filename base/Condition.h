#ifndef __BT_CONDITON_H
#define __BT_CONDITON_H
#include <pthread.h>
#include <boost/noncopyable.hpp>

#include "Mutex.h"

namespace bt {

class Cond : boost::noncopyable {
public:
    Cond(MutexLock& mutex)
        : mutex_(mutex)
    {
        pthread_cond_init(&cond_, NULL);
    }

    ~Cond()
    { pthread_cond_destroy(&cond_); }

    void wait()         { pthread_cond_wait(&cond_, mutex_.getPthreadMutex()); }
    void notify()       { pthread_cond_signal(&cond_); }
    void notify_all()   { pthread_cond_broadcast(&cond_); }

private:
    MutexLock& mutex_;
    pthread_cond_t cond_;
};

}

#endif

