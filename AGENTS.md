# Agent Guidelines for Ring Buffer Project

## Project Structure & Module Organization
- `src/` holds the lock-free queue primitives compiled into the `ringbuffer` static library; keep production code inside the `rb` namespace.
- Public headers live in `include/ringbuffer/` with paired `.inl.hpp` files included at the end of each header for template bodies.
- Tests mirror the production layout under `test/`; `test_ringbuffer_concur` covers multithreaded scenarios, while other suites target unit-level APIs.
- Treat `build/` as disposable output; never commit generated artifacts or Conan/CMake cache files.

## Build, Lint, and Test Commands
- **Install dependencies**: `conan install . --output-folder=build --build=missing -s build_type=Debug`
- **Configure project**: `cmake --preset default .`
- **Build all**: `cmake --build build`
- **Build specific target**: `cmake --build build --target <target_name>`
- **Run all tests**: `ctest --preset default --output-on-failure`
- **Run single test**: `./build/test/test_ringbuffer [test_name]` (Catch2 tag-based filtering)
- **Run concurrent tests**: `./build/test/test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096`
- **Format code**: `clang-format -i <file>` (LLVM style, 4-space indent, 120 char limit)

## Code Style Guidelines
- **Imports**: Project headers first, then standard library, then external (Boost, etc.)
- **Formatting**: Use `.clang-format` (LLVM base, 4-space indent, 120 char limit)
- **Types**: Use `std::size_t` for sizes, `std::string_view` for string parameters
- **Naming**: PascalCase for classes/structs, snake_case for functions/variables
- **Error handling**: Throw `std::runtime_error` for runtime contract breaches
- **Documentation**: Doxygen comments for public APIs (/// style)
- **Requirements**: T must be trivially copyable/destructible, ≤4KB, power-of-two capacity

## Testing Guidelines
- Catch2 drives the test harness; tag suites descriptively (e.g., `[ringbufferstore]`) so focused runs stay fast.
- Validate cache alignment, power-of-two capacity handling, and wraparound semantics explicitly in new tests.

## Commit & Pull Request Guidelines
- Write commits in imperative present tense (e.g., `add reader fence validation`) and keep unrelated changes separate; reference issues with `Refs #123` when applicable.
- Pull requests should summarize intent, list the tests executed, and call out any ABI or shared-memory risk; attach profiling data when claiming latency wins.

## Architecture Overview
- The ring buffer provides a single-producer, multi-consumer IPC channel backed by shared memory via `ring_buffer_store`.
- Capacity must be a power of two so mask arithmetic stays constant time, and payload types must be trivially destructible.
- Preserve 64-byte alignment for shared structures and rely on lock-free atomics to keep latency low under contention.
