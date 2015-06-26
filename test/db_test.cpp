#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "Options.h"
#include "Logger.h"
#include "DB.h"

using namespace bt;

int main()
{
    ILog4zManager::getRef().start();
    ILog4zManager::getRef().setLoggerLevel(LOG4Z_MAIN_LOGGER_ID,LOG_LEVEL_TRACE);

    LOGFMTT("Test db begin...");

    Options opts;
    
    DB* db = DB::open("test", opts);

    Slice ret;
    std::string keystr, valstr;
    char suf[32];
    for(int i = 0; i < 1024; i++) {
        sprintf(suf, "%010d", i);
	    keystr = std::string("hello") + "_" + std::string(suf);
	    valstr = std::string("chen") + "_" + std::string(suf);
        Slice key(keystr);
        Slice val(valstr);
        db->put(key, val);

        if(!db->get(key, ret)) {
            LOGFMTI("Cannot get value");
            return 0;
        }
        LOGFMTF("return a value [%s]", ret.data());
    }

    delete db;

    return 0;
}

