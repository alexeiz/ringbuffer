# Repository Guidelines

## Project Structure & Module Organization
- `src/` holds the lock-free queue primitives compiled into the `ringbuffer` static library; keep production code inside the `rb` namespace.
- Public headers live in `include/ringbuffer/` with paired `.inl.hpp` files included at the end of each header for template bodies.
- Tests mirror the production layout under `test/`; `test_ringbuffer_concur` covers multithreaded scenarios, while other suites target unit-level APIs.
- Treat `build/` as disposable output; never commit generated artifacts or Conan/CMake cache files.

## Build, Test, and Development Commands
- `conan install . --output-folder=build --build=missing -s build_type=Debug` installs third-party dependencies for local development.
- `cmake --preset default .` configures the project using the repo presets and caches toolchain options in `build/`.
- `cmake --build build` compiles the library and all test binaries; add `--target <name>` to focus on specific outputs.
- `ctest --preset default --output-on-failure` runs every configured test via CTest; pair with `-R <regex>` to scope specific suites.
- `./build/test/test_ringbuffer` runs the full Catch2 suite; append a tag such as `"[ringbuffer]"` to isolate a subset.
- `./build/test/test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096` stresses the concurrent path with tunable parameters.

## Coding Style & Naming Conventions
- Apply `.clang-format` (LLVM base, 4-space indent) before publishing updates; maintain include order: project headers, standard library, then external.
- Name classes/structs in PascalCase; functions, variables, and free utilities stay snake_case.
- Document public APIs with Doxygen comments and throw `std::runtime_error` for runtime contract breaches.

## Testing Guidelines
- Catch2 drives the test harness; tag suites descriptively (e.g., `[ringbufferstore]`) so focused runs stay fast.
- Validate cache alignment, power-of-two capacity handling, and wraparound semantics explicitly in new tests.
- Add concurrency fixtures to `test_ringbuffer_concur` when covering reader/writer regressions; keep scenarios deterministic.

## Commit & Pull Request Guidelines
- Write commits in imperative present tense (e.g., `add reader fence validation`) and keep unrelated changes separate; reference issues with `Refs #123` when applicable.
- Pull requests should summarize intent, list the tests executed, and call out any ABI or shared-memory risk; attach profiling data when claiming latency wins.

## Architecture Overview
- The ring buffer provides a single-producer, multi-consumer IPC channel backed by shared memory via `ring_buffer_store`.
- Capacity must be a power of two so mask arithmetic stays constant time, and payload types must be trivially destructible.
- Preserve 64-byte alignment for shared structures and rely on lock-free atomics to keep latency low under contention.
