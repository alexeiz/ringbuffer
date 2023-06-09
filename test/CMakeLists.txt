enable_testing()

find_package(Catch2 REQUIRED)

add_executable(test_ringbuffer
               ringbuffer.t.cpp
               ringbufferstore.t.cpp)
target_link_libraries(test_ringbuffer PRIVATE Catch2::Catch2WithMain ringbuffer)
