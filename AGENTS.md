# Repository Guidelines

## Project Structure & Module Organization
Source lives under `src/` with lock-free queue primitives compiled into the `ringbuffer` library. Public headers reside in `include/ringbuffer/`; template implementations pair with `.inl.hpp` files included at the header end. Tests mirror that layout in `test/`, and temporary outputs stay in `build/` (never commit generated files).

## Build, Test, and Development Commands
Install dependencies with `conan install . --output-folder=build --build=missing -s build_type=Debug`. Configure via `cmake --preset default .` and compile with `cmake --build build`. Run the full suite using `./build/test/test_ringbuffer`, or scope to a tag such as `./build/test/test_ringbuffer "[ringbuffer]"`. Exercise concurrent scenarios through `./build/test/test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096`.

## Coding Style & Naming Conventions
All production code stays inside the `rb` namespace. Apply `.clang-format` (LLVM base, 4-space indent) before committing and keep include order: project, standard library, external. Name classes and structs in PascalCase; functions, variables, and free functions remain snake_case. Document public APIs with Doxygen comments and raise runtime issues using `std::runtime_error`.

## Testing Guidelines
Catch2 drives the unit harness; tag suites descriptively (`[ringbufferstore]`) so focused runs stay fast. Add deterministic fixtures alongside features in `test/` and prefer verifying cache alignment and wrapping logic explicitly. Concurrency regressions belong in `test_ringbuffer_concur`; if coverage gaps remain, call them out in the PR description.

## Commit & Pull Request Guidelines
Write commits in imperative present tense (e.g., `add reader fence validation`) and keep unrelated changes separate. Reference issues with `Refs #123` when relevant and note any ABI or shared-memory risks in the message body. Pull requests should summarise intent, list tests executed, and link to profiling evidence for performance-sensitive updates.

## Architecture Overview
The ring buffer implements a single-producer, multi-consumer IPC channel backed by shared memory through `ring_buffer_store`. Capacity must be a power of two to exploit mask arithmetic, and payload types need to be trivially destructible. Preserve 64-byte alignment on shared structures and rely on lock-free atomics to uphold low-latency guarantees.
