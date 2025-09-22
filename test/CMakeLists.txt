add_executable(test_ringbuffer
               ringbuffer.t.cpp
               ringbufferstore.t.cpp)
target_link_libraries(test_ringbuffer PRIVATE ${TEST_LIBRARIES} ringbuffer)

add_test(NAME ringbuffer_tests COMMAND test_ringbuffer)

add_executable(test_ringbuffer_concur
               ringbuffer_concur.t.cpp)
target_link_libraries(test_ringbuffer_concur PRIVATE ${TEST_LIBRARIES} Boost::program_options ringbuffer)

add_test(NAME ringbuffer_concurrent_test
         COMMAND test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096 --use-threads)

# reader utility program (not a test)
add_executable(ringbuffer_reader
               ringbuffer_reader.t.cpp)
target_link_libraries(ringbuffer_reader PRIVATE ringbuffer)
