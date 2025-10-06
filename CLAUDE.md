# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System & Dependencies

This project uses CMake with Conan for dependency management.

### Build Commands

**Quick setup (requires just command runner):**
```bash
just conan-install
```

**Install dependencies manually:**
```bash
conan install . --output-folder=build/debug --build=missing -s build_type=Debug
conan install . --output-folder=build/release --build=missing -s build_type=Release
```

**Configure, build and run tests:**
```bash
cmake --preset debug
cmake --build build/debug --preset debug
ctest --preset debug
```

**Or with a single command:**
```bash
cmake --workflow --preset debug
```

**Run a specific test:**
```bash
./build/debug/test/test_ringbuffer
./build/debug/test/ringbuffer_reader
./build/debug/test/test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096 --use-threads
```

**Code formatting:**
```bash
clang-format -i include/ringbuffer/*.hpp test/*.t.cpp
```

## Code Architecture

The project implements a lock-free ring buffer for inter-process communication with the following key components:

### Core Classes

- `rb::ring_buffer<T>` - Single-producer, multiple-consumer ring buffer writer
- `rb::ring_buffer_reader<T>` - Reader for consuming data from the ring buffer
- `rb::ring_buffer_iterator<T>` - Input iterator interface for ring buffer readers
- `rb::ring_buffer_store` - Shared memory abstraction using boost::interprocess

**Key Methods Added constexpr:**
- `detail::ring_buffer_header::first()` - Extract first position from combined position value
- `detail::ring_buffer_header::last()` - Extract last position from combined position value
- `detail::ring_buffer_header::make_positions()` - Create combined position from first/last
- `ring_buffer::capacity()` - Returns buffer capacity
- `ring_buffer::empty()` - Checks if buffer is empty
- `ring_buffer_reader::empty()` - Checks if no items available
- `ring_buffer_iterator` constructor, equal(), increment() methods

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
- `test/*.t.cpp` - Catch2 test files

## Development Notes

- **Standards**: C++23 with strict compiler warnings (-Wall -Wextra -Werror)
- **Constraints**: Ring buffer capacity must be a power of 2
- **Limitations**: Items larger than system page size are not supported
- **Assumptions**: 64-byte cache line size
- **Memory Ordering**: Uses acquire/release semantic for lock-free operations
- **Position Encoding**: 64-bit value encodes first/last positions (32-bit each)

## Code Standards

**Formatting:**
- 4-space indentation, never tabs
- 120-character line limit
- LLVM-based style with custom brace wrapping
- Pointer alignment: `Type *ptr` (not `Type* ptr`)
- LF line endings, trim trailing whitespace

**Error Handling:**
- Throws exceptions for invalid operations
- Version compatibility checking between reader/writer
- Size compatibility checking between stored/read data types

## Testing

**Test Framework:** Catch2 v3.10.0
**Test Categories:**
- Basic functionality (`test_ringbuffer`)
- Reader operations (`ringbuffer_reader`)
- Concurrent operations (`test_ringbuffer_concur`)
- Memory store (`ringbufferstore`)

**Concurrent Testing:** Custom test harness in `ringbuffer_concur.cpp` for multi-process scenarios
