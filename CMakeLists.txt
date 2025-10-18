cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(ringbuffer
    VERSION 0.1.2
    DESCRIPTION "Lock-free ring buffer for inter-process communication"
    LANGUAGES CXX
)

# option to enable clang-tidy
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks" OFF)

# dependencies
include(cmake/deps.cmake)
include(cmake/clang-tidy.cmake)

# executable
add_library(ringbuffer INTERFACE)
target_link_libraries(ringbuffer INTERFACE ${LIBRARIES})

target_include_directories(ringbuffer
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ringbuffer INTERFACE cxx_std_23)

include(CTest)
if(BUILD_TESTING)
    message(STATUS "Build tests")
    enable_testing()
    add_subdirectory(test)
endif()

# Installation rules
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(cmake/genconfig.cmake)
include(cmake/install.cmake)
