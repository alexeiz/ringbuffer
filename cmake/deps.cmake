# system libraries
find_package(Threads REQUIRED)
list(APPEND LIBRARIES ${CMAKE_THREAD_LIBS_INIT})

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
list(APPEND LIBRARIES Boost::headers)
