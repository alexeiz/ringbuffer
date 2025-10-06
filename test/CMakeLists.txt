include("${CMAKE_SOURCE_DIR}/cmake/deps-test.cmake")

# Common compile options for all test targets
set(TEST_COMPILE_OPTIONS
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter
    -Wunused-variable
    -Wduplicated-cond
    -Wlogical-op
    -Wrestrict
    -ffast-math
)

# Clang-specific options
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND TEST_COMPILE_OPTIONS
        -stdlib=libc++
        -Wno-unknown-warning-option
    )
    list(APPEND TEST_LIBRARIES
        -lc++
        -lc++abi
    )
endif()

add_executable(test_ringbuffer
               ringbuffer.t.cpp
               ringbufferstore.t.cpp)
target_link_libraries(test_ringbuffer PRIVATE ${TEST_LIBRARIES} ringbuffer)
target_compile_options(test_ringbuffer PRIVATE ${TEST_COMPILE_OPTIONS})

add_test(NAME ringbuffer_tests COMMAND test_ringbuffer)

add_executable(test_ringbuffer_concur
               ringbuffer_concur.t.cpp)
target_link_libraries(test_ringbuffer_concur PRIVATE ${TEST_LIBRARIES} Boost::program_options ringbuffer)
target_compile_options(test_ringbuffer_concur PRIVATE ${TEST_COMPILE_OPTIONS})

add_test(NAME ringbuffer_concurrent_test
         COMMAND test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096 --use-threads)

# reader utility program (not a test)
add_executable(ringbuffer_reader
               ringbuffer_reader.t.cpp)
target_link_libraries(ringbuffer_reader PRIVATE ringbuffer)
target_compile_options(ringbuffer_reader PRIVATE ${TEST_COMPILE_OPTIONS})
