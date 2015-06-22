#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <sys/errno.h>

#include "RWLock.h"

using namespace bt;

static exception pthread_call_exception(const char *label, int result) {
    string what = "pthread call ";
    what += label;
    what += ", reason: ";
    what += strerror(result);
    return runtime_error(what);
}


static void pthread_call(const char* label, int result) {
  if (result != 0) {
    throw pthread_call_exception(label, result);
  }
}


RWLock::RWLock()
{
    pthread_call("init rwlock", pthread_rwlock_init(&rwlock_, NULL));
}

RWLock::~RWLock()
{
    pthread_call("destroy rwlock", pthread_rwlock_destroy(&rwlock_));
}

void RWLock::readLock()
{
    pthread_call("rdlock", pthread_rwlock_rdlock(&rwlock_));
}

bool RWLock::tryReadLock()
{
    int res = pthread_rwlock_tryrdlock(&rwlock_);
    if (res == 0) {
        return true;
    } else if (res == EBUSY) {
        return false;
    } else {
        throw pthread_call_exception("tryrdlock", res);
    }
}


void RWLock::writeLock()
{
    pthread_call("wrlock", pthread_rwlock_wrlock(&rwlock_));
}

bool RWLock::tryWriteLock()
{
    int res = pthread_rwlock_trywrlock(&rwlock_);
    if (res == 0) {
        return true;
    } else if (res == EBUSY) {
        return false;
    } else {
        throw pthread_call_exception("trywrlock", res);
    }
}

void RWLock::unlock()
{
    pthread_call("rwlock unlock", pthread_rwlock_unlock(&rwlock_));
}

