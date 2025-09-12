# Agent Instructions for Ring Buffer Project

## Build Commands
```bash
# Install dependencies
conan install . --output-folder=build --build=missing -s build_type=Debug

# Configure and build
cmake --preset default .
cmake --build build

# Run all tests
./build/test/test_ringbuffer

# Run specific test suites
./build/test/test_ringbuffer "[ringbuffer]"      # Core ring buffer tests
./build/test/test_ringbuffer "[ringbufferstore]" # Shared memory store tests

# Run concurrent tests
./build/test/test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096
```

## Code Style Guidelines
- **Namespace**: All code in `tla` namespace
- **Headers**: Use `#pragma once`, include order: project headers > stdlib > external
- **Types**: PascalCase classes/structs, snake_case functions/variables
- **Templates**: Implementation in `.inl.hpp` files included at header end
- **Error Handling**: Use `std::runtime_error` for runtime errors
- **Memory**: Lock-free atomics, 64-byte cache line alignment
- **Compiler**: C++23 with `-Wall -Wextra -Werror -Wno-unused-parameter`
- **Documentation**: Use Doxygen-style comments for public APIs
- **Includes**: Group includes (project, stdlib, external) with blank lines

## Architecture Notes
- Lock-free ring buffer for IPC using shared memory
- Capacity must be power of 2, items must be trivially destructible
- Single producer, multiple consumers pattern
- Use `ring_buffer_store` for shared memory management


