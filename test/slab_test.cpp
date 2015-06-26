#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "Logger.h"
#include "Slab.h"

using namespace std;
using namespace bt;

int main()
{
    ILog4zManager::getRef().start();
    ILog4zManager::getRef().setLoggerLevel(LOG4Z_MAIN_LOGGER_ID,LOG_LEVEL_TRACE);

    LOGFMTT("Test slab begin...");

    Slab s = Slab();
    s.init(4 * 1024 * 1024);
    char* p = NULL;
    for(int i = 0; i < 1000; i++) {
        p = (char*)s.alloc(14 + i % 1024);
        if(p == NULL) {
            LOGFMTT("Alloc NULL potter [i: %d]", i);
            return -1;
        }
        strcpy(p, "hello");
        //s.free((void*)p);
    }
    s.slabStat();

    LOGFMTT("Test slab end...");

    return 0;
}
