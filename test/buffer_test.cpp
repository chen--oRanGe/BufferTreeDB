#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <errno.h>

#include "Options.h"
#include "Logger.h"
#include "DB.h"
#include "Slab.h"
#include "Buffer.h"

using namespace bt;

const char* path = "./test_buffer.txt";
static int fd = -1;

bool openFile()
{
    fd = open(path, O_RDWR | O_CREAT);
    if(fd < 0) {
        printf("open error fd [%d] error [%d]\n", fd, errno);
        return false;
    }
    return true;
}

void writeFile(Buffer& buf, size_t size)
{
    printf("writeFile: %lu\n", size);
    int ret = pwrite(fd, buf.beginRead(), size, 0);
    if(ret != size) {
        printf("read file error\n");
        return;
    }
    buf.updateReadIndex(size);
}

void readFile(Buffer& buf, size_t size)
{
    printf("readFile: %lu\n", size);
    int ret = pread(fd, buf.beginWrite(), size, 0);
    if(ret != size) {
        printf("read file error\n");
        return;
    }
    buf.updateWriterIndex(size);
}

int main()
{
    //ILog4zManager::getRef().start();
    //ILog4zManager::getRef().setLoggerLevel(LOG4Z_MAIN_LOGGER_ID,LOG_LEVEL_TRACE);


    //LOGFMTT("Test buffer begin...");

    Buffer buf, buf1;

    buf.ensureWritableBytes(1024);
    buf1.ensureWritableBytes(1024);

    buf.appendInt8(1);
    buf.appendInt32(2);
    buf.appendInt64(3);
    std::string str("hello world");
    buf.appendString(str);
    
    if(!openFile()) {
        return -1;
    }

    size_t size = buf.readableBytes();
    writeFile(buf, size);

    readFile(buf1, size);
    int r1 = buf1.readInt8();
    int r2 = buf1.readInt32();
    int r3 = buf1.readInt64();
    std::string r4 = buf1.readString();

    printf("after readFile %d, %d, %ld, %s\n", r1, r2, r3, r4.c_str());

    close(fd);
    return 0;
}
