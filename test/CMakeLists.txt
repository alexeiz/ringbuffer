enable_testing()

find_package(Catch2 REQUIRED)

# Catch2 test executable (contains TEST_CASE macros)
add_executable(test_ringbuffer
               ringbuffer.t.cpp
               ringbufferstore.t.cpp)
target_link_libraries(test_ringbuffer PRIVATE Catch2::Catch2WithMain ringbuffer)

# Register Catch2 tests with CTest - basic approach
add_test(NAME ringbuffer_tests COMMAND test_ringbuffer)

# Standalone concurrent test program
add_executable(test_ringbuffer_concur
               ringbuffer_concur.t.cpp)
target_link_libraries(test_ringbuffer_concur PRIVATE ringbuffer Boost::program_options)

# Register concurrent test with CTest
add_test(NAME ringbuffer_concurrent_test
         COMMAND test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096)

# Utility program (not a test)
add_executable(ringbuffer_reader
               ringbuffer_reader.t.cpp)
target_link_libraries(ringbuffer_reader PRIVATE ringbuffer)


