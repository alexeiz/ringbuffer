cmake_minimum_required (VERSION 3.28)

project (ringbuffer)
include("FindPkgConfig")

# set build type
set(default_build_type "Debug")
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to '${default_build_type}'")

  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose build type." FORCE)

  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
      STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# set compiler flags
set(CMAKE_CXX_STANDARD   23)
set(CMAKE_CXX_EXTENSIONS OFF)
string(REPLACE "-std=c++11" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wunused-variable")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wduplicated-cond -Wlogical-op")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wrestrict")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")

# Clang-specific
if(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wno-unknown-warning-option")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
endif()

# dependencies
include(cmake/deps.cmake)

# include directories and source files
include_directories(include)
set(SOURCES
    src/ringbufferstore.cpp)

# executable
add_library(ringbuffer ${SOURCES})
target_link_libraries(ringbuffer PRIVATE ${LIBRARIES})

include(CTest)
if(BUILD_TESTING)
  message(STATUS "Build tests")

  add_definitions(-DBUILD_TESTING)
  include(cmake/deps-test.cmake)

  add_subdirectory(test)
else()
  message(STATUS "Skip tests")
endif()

#INSTALL(
#  TARGETS ringbuffer
#  DIRECTORY ${CMAKE_SOURCE_DIR}/include
#  FILES_MATCHING PATTERN "*.h*"
#  INCLUDES DESTINATION include
#  LIBRARY DESTINATION lib)
