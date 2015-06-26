set(base_SRCS
    RWLock.cpp
    Thread.cpp
    Logger.cpp
    )

add_library(BufferTreeDBBase ${base_SRCS})
target_link_libraries(BufferTreeDBBase pthread rt)

install(TARGETS BufferTreeDBBase DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/base)
