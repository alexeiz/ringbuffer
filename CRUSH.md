# CRUSH.md

This file provides guidance to Crush agents working in this ring buffer repository.

## Build Commands
```bash
# Install dependencies
conan install . --output-folder=build --build=missing -s build_type=Debug

# Configure and build
cmake --preset default .
cmake --build build

# Run all tests
./build/test/test_ringbuffer

# Run specific test (Catch2 example)
./build/test/test_ringbuffer "[ring_buffer]"
```

## Lint and Typecheck
- Linting is enforced via compiler flags: C++23 with -Wall -Wextra -Werror
- No separate lint command; warnings treated as errors during build
- Ensure no unused parameters/variables

## Test Commands
- Uses Catch2 framework
- Run single test: ./build/test/test_ringbuffer "[test_name]"
- Example: ./build/test/test_ringbuffer "[ringbufferstore]"

## Code Style Guidelines
- **Namespace**: All code in `rb` namespace
- **Headers**: Use `#pragma once`; include order: project headers, stdlib, external
- **Types**: PascalCase for classes/structs, snake_case for functions/variables
- **Templates**: Implement in `.inl.hpp` files, include at end of headers
- **Formatting**: Standard C++ indentation; no specific formatter mentioned
- **Error Handling**: Use `std::runtime_error` for runtime errors
- **Memory/Threading**: Lock-free using atomics; 64-byte cache line alignment
- **Conventions**: Capacity power of 2; T trivially destructible; single producer, multiple consumers

## Architecture Notes
- Lock-free ring buffer for IPC via shared memory (boost::interprocess)
- Core classes: rb::ring_buffer<T>, rb::ring_buffer_reader<T>, rb::ring_buffer_store
- File org: include/*.hpp, src/*.cpp, test/*.t.cpp
- Assumptions: 64-byte cache lines; items <= page size
