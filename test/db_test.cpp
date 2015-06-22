#include "Options.h"
#include "Logger.h"
#include "DB.h"

using namespace bt;

int main()
{
    Options opts;
    
    DBImpl* db = open("test", opts);
    
    Slice key("hello");
    Slice val("world");
    db->put(key, val);

    Slice ret();
    db->get(key, ret);
    LOGFMTI("return a value [%s]", ret.data());

    delete db;

    return 0;
}

