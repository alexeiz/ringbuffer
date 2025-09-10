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

# Run specific test (example)
./build/test/test_ringbuffer "[ring_buffer]"
```

## Code Style Guidelines
- **Namespace**: All code in `tla` namespace
- **Headers**: Use `#pragma once`, include order: project headers first, then stdlib, then external
- **Types**: PascalCase for classes/structs, snake_case for functions/variables
- **Templates**: Implementation in `.inl.hpp` files included at end of headers
- **Error Handling**: Use exceptions (`std::runtime_error`) for runtime errors
- **Memory**: Lock-free atomics for thread safety, 64-byte cache line alignment
- **Compiler**: C++23 with `-Wall -Wextra -Werror`, no unused parameters/variables

## Architecture Notes
- Lock-free ring buffer for IPC using shared memory
- Capacity must be power of 2, items must be trivially destructible
- Single producer, multiple consumers pattern
- Use `ring_buffer_store` for shared memory management