# Agent Guidelines for Ring Buffer Project

## Project Structure & Module Organization
- **Core Library**: **Header-only** library with all implementation in `include/ringbuffer/`; keep production code inside the `rb` namespace.
- **Public API**: Headers live in `include/ringbuffer/` with paired `.inl.hpp` files included at the end of each header for template implementations.
- **Testing**: Tests mirror the production layout under `test/`; `test_ringbuffer_concur` covers multi-process/thread scenarios, while other suites target unit-level APIs.
- **Build System**: `cmake/` contains dependency management; `build/debug/` and `build/release/` are disposable output directories - never commit generated artifacts or Conan/CMake cache files.
- **Configuration**: Project uses Conan for dependencies (Boost 1.88.0, Catch2 3.10.0) and CMake presets for standardized builds with separate debug/release configurations.

## Build, Lint, and Test Commands

### Build Commands
- **Configure, build, and test (all-in-one)**: `cmake --workflow --preset debug` or `cmake --workflow --preset release`
- **Configure project**: `cmake --preset debug` or `cmake --preset release`
- **Build all**: `cmake --build build --preset debug` or `cmake --build build --preset release`
- **Build specific target**: `cmake --build build --preset debug --target <target_name>`
- **Run all tests**: `ctest --preset debug --output-on-failure` or `ctest --preset release --output-on-failure`
- **Install library**: `cmake --install build/debug` or `cmake --install build/release` (installs headers and CMake config)

### Test Commands
- **Run single test suite**: `./build/debug/test/test_ringbuffer [test_name]` (Catch2 tag-based filtering: `[ringbufferstore]`, `[basic]`, etc.)
- **Run concurrent tests**: `./build/debug/test/test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096`
- **Run reader utility**: `./build/debug/test/ringbuffer_reader` (standalone reader program, not a test)

### Code Quality Commands
- **Format code**: `clang-format -i <file>` (uses `.clang-format`: LLVM base, 4-space indent, 120 char limit, custom brace wrapping, `PointerAlignment: Middle`)
- **Run clang-tidy**: Configure with `-DENABLE_CLANG_TIDY=ON`, uses `.clang-tidy` configuration
- **Clean build**: Remove `build/debug/` and/or `build/release/` directories to start fresh

## Code Style Guidelines
- **C++ Standard**: C++23 with strict compiler warnings (`-Wall -Wextra -Werror`)
- **Library Type**: Header-only library - all implementation in headers (`.hpp` and `.inl.hpp` files)
- **Include Order**: Project headers first (`"ringbuffer/*.hpp"`), then standard library (`<cstddef>`, `<atomic>`, etc.), then external dependencies (Boost, Catch2)
- **Formatting**: Use `.clang-format` config (LLVM base, 4-space indent, 120 char limit, custom brace wrapping with `PointerAlignment: Middle`)
- **Static Analysis**: Use `.clang-tidy` config when enabled (comprehensive checks with low-level construct exceptions for lock-free/shared-memory code)
- **Types**: Use `std::size_t` for sizes/counts, `std::string_view` for string parameters, concepts for template constraints
- **Naming**:
  - `snake_case` for classes/structs: `ring_buffer`, `ring_buffer_reader`
  - `snake_case` for functions/variables/namespaces: `push()`, `get_size()`, `rb::`
  - `snake_case` for compile-time constants: `ring_buffer_cache_linesize`, `system_page_size`
- **Error Handling**: Throw `std::runtime_error` for runtime contract violations, use Boost exceptions for IPC failures
- **Documentation**: Doxygen-style comments (`///`) for public APIs, inline comments for complex algorithms
- **Template Requirements**: Use C++20 concepts (`ring_buffer_value`) to constrain types T to be trivially copyable/destructible and ≤4KB (system page size)

## Testing Guidelines
- **Framework**: Catch2 v3.10.0 drives all test harnesses; use descriptive tags (e.g., `[ringbufferstore]`, `[basic]`, `[concurrent]`) for focused test runs
- **Test Structure**:
  - `test_ringbuffer`: Unit tests for core ring buffer functionality and store operations (combines `ringbuffer.t.cpp` and `ringbufferstore.t.cpp`)
  - `test_ringbuffer_concur`: Multi-process/thread concurrent testing with configurable parameters (`ringbuffer_concur.t.cpp`)
  - `ringbuffer_reader`: Standalone reader utility program (not a test, in `ringbuffer_reader.t.cpp`)
- **Test Categories**: Basic functionality, reader operations, concurrent scenarios, shared memory store validation
- **Validation Points**: Cache alignment (64-byte boundaries), power-of-two capacity constraints, wraparound semantics, version compatibility, and lock-free atomic operations
- **Performance Testing**: Use concurrent test harness with `--use-threads` flag for threading vs `--readers=N` for multi-process scenarios
- **Build Configuration**: Tests support both debug and release builds with separate executables in `build/debug/test/` and `build/release/test/`

## Commit & Pull Request Guidelines
- Write commits in imperative present tense (e.g., `add reader fence validation`, `convert to header-only library`) and keep unrelated changes separate
- Reference issues with `Refs #123` when applicable
- Pull requests should summarize intent, list the tests executed, and call out any ABI or shared-memory compatibility risks
- For header-only library changes, note any impacts on compile times or template instantiation
- Attach profiling data when claiming latency or throughput improvements
- Tag breaking changes clearly (version compatibility, API changes, shared memory layout changes)

## Architecture Overview
- **Core Design**: Single-producer, multiple-consumer lock-free ring buffer with inter-process communication via shared memory
- **Library Type**: Header-only library (converted from static library) - entire implementation in headers for zero-overhead abstraction
- **Key Classes**:
  - `rb::ring_buffer<T>` - Writer/producer interface with `push()`, `emplace()`, and capacity management
  - `rb::ring_buffer_reader<T>` - Reader/consumer interface with `get()`, `next()`, and range-based iteration
  - `rb::ring_buffer_iterator<T>` - STL-compatible input iterator for range-based for loops
  - `rb::ring_buffer_store` - Shared memory abstraction using boost::interprocess
- **Memory Layout**: 64-byte cache-aligned structures with atomic position tracking via combined 64-bit values (32-bit first + 32-bit last positions)
- **Key Constants**:
  - `system_page_size = 4096` - System page size in bytes (maximum item size)
  - `ring_buffer_cache_linesize = 64` - Cache line size for alignment
  - `ring_buffer_version = 1` - Version number for reader/writer compatibility checking
- **Constraints**:
  - Capacity must be power-of-two for efficient modulo operations via bit masking
  - Types T must satisfy `ring_buffer_value` concept (trivially copyable/destructible, ≤4KB system page size)
  - Memory alignment assumes 64-byte cache lines
  - Uses acquire/release memory ordering for lock-free synchronization
- **API Patterns**: RAII-based shared memory management, iterator-based consumption, exception-based error reporting
- **constexpr Support**: Many methods marked `constexpr` for compile-time evaluation where possible (capacity calculations, position encoding/decoding)

## Dependencies & External Libraries
- **Conan Package Manager**: Used for dependency management, configured in `conanfile.txt`
  - `boost/1.88.0` - Provides interprocess communication primitives (boost::interprocess)
  - `catch2/3.10.0` - Test framework for unit and integration tests
  - Boost program_options used for concurrent test command-line argument parsing
- **CPM (CMake Package Manager)**: Used for additional dependencies via `cmake/CPM.cmake`
  - `scope-exit@0.2.3` - RAII scope guard utilities (from github.com/alexeiz/scope-exit)
- **CMake Build System**: Uses presets pattern with `CMakePresets.json` for standardized configurations
  - Includes generated presets from `build/debug/CMakePresets.json` and `build/release/CMakePresets.json` (created by Conan)
  - Workflow presets for one-command configure+build+test
- **Build Toolchain**: Supports both GCC (default) and Clang with C++23 standard
- **Optional Tools**:
  - `clang-format` for code formatting (`.clang-format` config)
  - `clang-tidy` for static analysis (`.clang-tidy` config, enable with `-DENABLE_CLANG_TIDY=ON`)
  - `ctest` for test execution

## Development Workflow & Best Practices
- **Before Changes**: Always run existing tests to establish baseline: `ctest --preset debug --output-on-failure`
- **During Development**:
  - Use `cmake --build build --preset debug` for incremental builds
  - Use `cmake --workflow --preset debug` for full configure-build-test cycle
  - Leverage separate debug/release builds for performance comparison
- **Code Changes**:
  - Format code with `clang-format -i` before commits; follow existing naming conventions
  - Run clang-tidy for static analysis when enabled (`-DENABLE_CLANG_TIDY=ON`)
  - Keep changes surgical and minimal, especially in header-only implementation
- **Testing Strategy**:
  - Add unit tests for new APIs in appropriate test files
  - Use concurrent tests for race condition validation
  - Test both debug and release builds when performance-critical
- **Error Handling**: Prefer exceptions over error codes; validate preconditions at API boundaries
- **Performance**: Profile concurrent scenarios with `test_ringbuffer_concur` utility before claiming performance improvements
- **Installation**: Test package installation with `cmake --install build/debug --prefix /tmp/test-install` to verify CMake config correctness

## Tooling Configuration Files
- **`.clang-format`**: LLVM-based style with 4-space indent, 120-char limit, `PointerAlignment: Middle`, custom brace wrapping
- **`.clang-tidy`**: Comprehensive checks (clang-analyzer, bugprone, performance, readability, modernize, cppcoreguidelines) with exceptions for:
  - Low-level constructs (unions, reinterpret_cast, pointer arithmetic) used in lock-free/shared-memory code
  - Naming conventions (follows project's snake_case everywhere)
  - Magic numbers (cache line size, page size, etc. are intentional)
- **`.editorconfig`**: Universal settings (LF line endings, trim trailing whitespace, 120-char limit)
- **`.gitignore`**: Excludes `build/`, `build-release/`, `.cache/`, `.vscode/`, `.vs/`, `.aider*`, compiled objects, and executables
