# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System & Dependencies

This project uses CMake with Conan for dependency management.

### Build Commands

Install dependencies:
```bash
conan install . --output-folder=build --build=missing -s build_type=Debug
```

Configure and build:
```bash
cmake --preset default
cmake --build build --preset default
```

### Running Tests

The project uses Catch2 testing framework. Tests are built with the main project and can be run with:
```bash
ctest --preset default --output-on-failure
```

## Code Architecture

The project implements a lock-free ring buffer for inter-process communication with the following key components:

### Core Classes

- `rb::ring_buffer<T>` - Single-producer, multiple-consumer ring buffer writer
- `rb::ring_buffer_reader<T>` - Reader for consuming data from the ring buffer
- `rb::ring_buffer_iterator<T>` - Input iterator interface for ring buffer readers
- `rb::ring_buffer_store` - Shared memory abstraction using boost::interprocess

### Architecture Details

- **Namespace**: All code is in the `rb` namespace
- **Memory Management**: Uses shared memory objects for inter-process communication
- **Thread Safety**: Lock-free design using atomic operations for position tracking
- **Data Alignment**: Cache-line aligned data structures (64-byte alignment)
- **Template Requirements**: Type T must be trivially destructible

### File Organization

- `include/ringbuffer.hpp` - Main ring buffer class declarations
- `include/ringbuffer.inl.hpp` - Template implementation details
- `include/ringbufferstore.hpp` - Shared memory store implementation
- `src/ringbufferstore.cpp` - Store implementation (minimal, mostly header-only)
- `test/*.t.cpp` - Catch2 test files

## Development Notes

- C++23 standard with strict compiler warnings (-Wall -Wextra -Werror)
- Ring buffer capacity must be a power of 2
- Items larger than system page size are not supported
- The implementation assumes 64-byte cache line size
