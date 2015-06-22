#ifndef __BT_THREAD_H
#define __BT_THREAD_H

#include <pthread.h>
#include <sys/syscall.h>

#include <string>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

using namespace std;

namespace bt {

class Thread : boost::noncopyable 
{
public:
    typedef boost::function<void()> Function;

    explicit Thread(const Function&, const string& name = string());
    ~Thread();

    void start();
    void join();
    bool started() const
    { return started_; }

    pid_t getTid()
    {
        return static_cast<pid_t>(*tid_);
    }

    const string& name() const
    { return name_; }

    bool is_main_thread() 
    {
        return getTid() == getpid();
    }
private:
    bool started_;
    bool joined_;
    pthread_t pthreadId_;
    string name_;
    boost::shared_ptr<pid_t> tid_;
    Function func_;
};

namespace currentData {

    extern __thread const char* threadName;
    extern __thread pid_t cachedTid;
    extern pid_t getTid();

}

}

#endif
