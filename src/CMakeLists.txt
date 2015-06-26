set(BufferTreeDB_SRCS
    BufferTree.cpp
    Cache.cpp
    DBImpl.cpp
    Layout.cpp
    Msg.cpp
    Node.cpp
    Slab.cpp
    )

add_library(BufferTreeDB SHARED ${BufferTreeDB_SRCS})
target_link_libraries(BufferTreeDB BufferTreeDBBase)

install(TARGETS BufferTreeDB DESTINATION lib)

set(HEADERS
    BufferTree.h
    Cache.h
    Comparator.h
    DBImpl.h
    Layout.h
    Msg.h
    Node.h
    Options.h
    Skiplist.h
    Slab.h
    )
install(FILES ${HEADERS} DESTINATION include/src)
