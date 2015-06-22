#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

#include <boost/weak_ptr.hpp>

#include "Thread.h"

namespace bt {
namespace currentData {
__thread pid_t cachedTid = 0;
__thread const char* threadName = "unknown";

pid_t getTid()
{
    if(!cachedTid) {
        cachedTid = static_cast<pid_t>(syscall(SYS_gettid));
    }
    return cachedTid;
}
}
}


using namespace bt;

struct ThreadData
{
    typedef boost::function<void()> Function;
    Function fn_;
    std::string name_;
    boost::weak_ptr<pid_t> weakTid_;
    ThreadData(Function& fn, std::string& name, boost::shared_ptr<int> tid)
        : fn_(fn),
          name_(name),
          weakTid_(tid)
    {}

    void runInData()
    {
        boost::shared_ptr<pid_t> sharedTid = weakTid_.lock();
        if(sharedTid) {
            *sharedTid = currentData::getTid();
            sharedTid.reset();
        }
        currentData::threadName = name_.c_str();
        fn_();
        currentData::threadName = "thread finished";
    }
};

void* func(void* arg)
{
    ThreadData* data = (ThreadData*)arg;
    data->runInData();
    delete data;

    return NULL;
}

Thread::Thread(const Function& fn, const std::string& name)
    : started_(false),
      joined_(false),
      pthreadId_(0),
      name_(name),
      tid_(new pid_t(0)),
      func_(fn)
{}

Thread::~Thread()
{
    if(started_ && !joined_)
        pthread_detach(pthreadId_);
}

void Thread::start()
{
    ThreadData* data = new ThreadData(func_, name_, tid_);
    started_ = true;
    if(pthread_create(&pthreadId_, NULL, &func, (void*)data)) {
        started_ = false;
        delete data;
    }
}

void Thread::join()
{
    assert(started_);
    assert(!joined_);

    joined_ = true;
    pthread_join(pthreadId_, NULL);
}
