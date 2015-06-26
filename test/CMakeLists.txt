add_executable(db_test db_test.cpp)
target_link_libraries(db_test BufferTreeDB)

add_executable(skiplist_test skiplist_test.cpp)
target_link_libraries(skiplist_test BufferTreeDB)

add_executable(slab_test slab_test.cpp)
target_link_libraries(slab_test BufferTreeDB)

add_executable(buffer_test buffer_test.cpp)
target_link_libraries(buffer_test BufferTreeDB)
