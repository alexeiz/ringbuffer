cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(ringbuffer
    VERSION 1.0.0
    DESCRIPTION "Lock-free ring buffer for inter-process communication"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# dependencies
include(cmake/deps.cmake)

# include directories and source files
include_directories(include)
set(SOURCES
    src/ringbufferstore.cpp)

# executable
add_library(ringbuffer ${SOURCES})
target_link_libraries(ringbuffer PRIVATE ${LIBRARIES})

target_include_directories(ringbuffer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ringbuffer PUBLIC cxx_std_23)
target_compile_options(ringbuffer PRIVATE
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
    target_compile_options(ringbuffer PRIVATE
        -stdlib=libc++
        -Wno-unknown-warning-option
    )
    target_link_libraries(ringbuffer PRIVATE
        -stdlib=libc++
        -lc++abi
    )
endif()

include(CTest)
if(BUILD_TESTING)
    message(STATUS "Build tests")
    enable_testing()
    add_subdirectory(test)
endif()

# Installation rules
include(GNUInstallDirs)

install(TARGETS ringbuffer
    EXPORT ringbufferTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ringbufferTargets
    FILE ringbufferTargets.cmake
    NAMESPACE ringbuffer::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ringbuffer
)

# Generate and install package configuration
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ringbufferConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ringbufferConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ringbufferConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ringbuffer
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ringbufferConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ringbufferConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ringbuffer
)
