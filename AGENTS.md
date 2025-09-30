# Agent Guidelines for Ring Buffer Project

## Project Structure & Module Organization
- **Core Library**: `src/` contains minimal implementation compiled into the `ringbuffer` static library; keep production code inside the `rb` namespace.
- **Public API**: Headers live in `include/ringbuffer/` with paired `.inl.hpp` files included at the end of each header for template implementations.
- **Testing**: Tests mirror the production layout under `test/`; `test_ringbuffer_concur` covers multi-process/thread scenarios, while other suites target unit-level APIs.
- **Build System**: `cmake/` contains dependency management; `build/` is disposable output - never commit generated artifacts or Conan/CMake cache files.
- **Configuration**: Project uses Conan for dependencies (Boost 1.88.0, Catch2 3.10.0) and CMake presets for standardized builds.

## Build, Lint, and Test Commands
- **Install dependencies**: `conan install . --output-folder=build --build=missing -s build_type=Debug`
- **Configure project**: `cmake --preset default`
- **Build all**: `cmake --build build --preset default`
- **Build specific target**: `cmake --build build --target <target_name>`
- **Run all tests**: `ctest --preset default --output-on-failure`
- **Run single test suite**: `./build/test/test_ringbuffer [test_name]` (Catch2 tag-based filtering: `[ringbufferstore]`, `[basic]`, etc.)
- **Run concurrent tests**: `./build/test/test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096`
- **Run reader utility**: `./build/test/ringbuffer_reader` (standalone reader program, not a test)
- **Format code**: `clang-format -i <file>` (uses `.clang-format`: LLVM base, 4-space indent, 120 char limit, custom brace wrapping)
- **Clean build**: Remove `build/` directory to start fresh

## Code Style Guidelines
- **C++ Standard**: C++23 with strict compiler warnings (`-Wall -Wextra -Werror`)
- **Include Order**: Project headers first (`"ringbuffer/*.hpp"`), then standard library (`<cstddef>`, `<atomic>`, etc.), then external dependencies (Boost, Catch2)
- **Formatting**: Use `.clang-format` config (LLVM base, 4-space indent, 120 char limit, custom brace wrapping with `PointerAlignment: Middle`)
- **Types**: Use `std::size_t` for sizes/counts, `std::string_view` for string parameters, concepts for template constraints
- **Naming**:
  - `snake_case` for classes/structs: `ring_buffer`, `ring_buffer_reader`
  - `snake_case` for functions/variables/namespaces: `push()`, `get_size()`, `rb::`
  - `snake_case` for compile-time constants: `ring_buffer_cache_linesize`
- **Error Handling**: Throw `std::runtime_error` for runtime contract violations, use Boost exceptions for IPC failures
- **Documentation**: Doxygen-style comments (`///`) for public APIs, inline comments for complex algorithms
- **Template Requirements**: Use C++20 concepts (`ring_buffer_value`) to constrain types T to be trivially copyable/destructible and ≤4KB

## Testing Guidelines
- **Framework**: Catch2 v3.10.0 drives all test harnesses; use descriptive tags (e.g., `[ringbufferstore]`, `[basic]`, `[concurrent]`) for focused test runs
- **Test Structure**:
  - `test_ringbuffer`: Unit tests for core ring buffer functionality and store operations
  - `test_ringbuffer_concur`: Multi-process/thread concurrent testing with configurable parameters
  - `ringbuffer_reader`: Standalone reader utility program (not a test)
- **Test Categories**: Basic functionality, reader operations, concurrent scenarios, shared memory store validation
- **Validation Points**: Cache alignment (64-byte boundaries), power-of-two capacity constraints, wraparound semantics, version compatibility, and lock-free atomic operations
- **Performance Testing**: Use concurrent test harness with `--use-threads` flag for threading vs `--readers=N` for multi-process scenarios

## Commit & Pull Request Guidelines
- Write commits in imperative present tense (e.g., `add reader fence validation`) and keep unrelated changes separate; reference issues with `Refs #123` when applicable.
- Pull requests should summarize intent, list the tests executed, and call out any ABI or shared-memory risk; attach profiling data when claiming latency wins.

## Architecture Overview
- **Core Design**: Single-producer, multiple-consumer lock-free ring buffer with inter-process communication via shared memory
- **Key Classes**:
  - `rb::ring_buffer<T>` - Writer/producer interface with `push()`, `emplace()`, and capacity management
  - `rb::ring_buffer_reader<T>` - Reader/consumer interface with `get()`, `next()`, and range-based iteration
  - `rb::ring_buffer_iterator<T>` - STL-compatible input iterator for range-based for loops
  - `rb::ring_buffer_store` - Shared memory abstraction using boost::interprocess
- **Memory Layout**: 64-byte cache-aligned structures with atomic position tracking via combined 64-bit values (32-bit first + 32-bit last positions)
- **Constraints**:
  - Capacity must be power-of-two for efficient modulo operations via bit masking
  - Types T must satisfy `ring_buffer_value` concept (trivially copyable/destructible, ≤4KB)
  - Memory alignment assumes 64-byte cache lines
  - Uses acquire/release memory ordering for lock-free synchronization
- **API Patterns**: RAII-based shared memory management, iterator-based consumption, exception-based error reporting

## Dependencies & External Libraries
- **Conan Package Manager**: Used for dependency management, configured in `conanfile.txt`
  - `boost/1.88.0` - Provides interprocess communication primitives
  - `catch2/3.10.0` - Test framework for unit and integration tests
- **CMake Build System**: Uses presets pattern with `CMakeUserPresets.json` for standardized configurations
- **Build Toolchain**: Supports both GCC (default) and Clang with C++23 standard
- **Optional Tools**: `clang-format` for code formatting, `ctest` for test execution

## File Organization & Key Files
```
├── include/ringbuffer/          # Public API headers
│   ├── ringbuffer.hpp          # Main ring buffer classes
│   ├── ringbuffer.inl.hpp      # Template implementations
│   └── ringbufferstore.hpp     # Shared memory store
├── src/                        # Minimal library implementation
│   └── ringbufferstore.cpp     # Store implementation (mostly header-only)
├── test/                       # Test suite
│   ├── ringbuffer.t.cpp        # Core functionality tests
│   ├── ringbuffer_reader.t.cpp # Reader utility program
│   ├── ringbuffer_concur.t.cpp # Concurrent/multi-process tests
│   └── ringbufferstore.t.cpp   # Shared memory store tests
├── cmake/                      # Build system configuration
│   ├── deps.cmake              # Dependency management
│   └── get_cpm.cmake           # CPM package manager
├── .clang-format               # Code formatting rules
├── conanfile.txt               # Package dependencies
├── CMakeUserPresets.json       # Build presets (default, release)
└── README.md                   # Usage examples and API documentation
```

## Development Workflow & Best Practices
- **Before Changes**: Always run existing tests to establish baseline: `ctest --preset default --output-on-failure`
- **During Development**: Use `cmake --build build --preset default` for incremental builds
- **Code Changes**: Format code with `clang-format -i` before commits; follow existing naming conventions
- **Testing Strategy**: Add unit tests for new APIs, use concurrent tests for race condition validation
- **Error Handling**: Prefer exceptions over error codes; validate preconditions at API boundaries
- **Performance**: Profile concurrent scenarios with `test_ringbuffer_concur` utility before claiming performance improvements
