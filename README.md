# ringbuffer

A high-performance, lock-free ring buffer implementation for inter-process communication (IPC) in C++23. This library provides a single-producer, multiple-consumer queue where consumers can observe items without removing them from the queue.

## Design and Implementation

### Architecture Overview

The ring buffer is built on three core components that work together to provide efficient lock-free inter-process communication:

**`ring_buffer<T>`** serves as the writer interface with exclusive write access. A single producer pushes items to the buffer, automatically overwriting the oldest data when capacity is reached. The implementation ensures that consumers never see partially written items through careful use of memory ordering semantics.

**`ring_buffer_reader<T>`** provides independent read access for multiple consumers. Each reader maintains its own position in the buffer and can process items at its own pace. Readers that fall too far behind are automatically advanced to prevent stale data access, jumping ahead by a configurable amount to resume from a valid position.

**`ring_buffer_store`** abstracts the shared memory management layer using Boost.Interprocess. It handles the creation, opening, and mapping of POSIX shared memory objects, providing a clean interface for both writers and readers to access the same memory region across process boundaries.

### Lock-Free Synchronization

The implementation achieves thread-safety without locks through atomic operations and careful memory ordering. The core synchronization mechanism uses a single 64-bit atomic value that encodes both the first and last positions of the valid data range. This combined representation allows the writer to update both positions atomically, ensuring readers always see a consistent view of the buffer state.

The writer uses release semantics when updating positions, guaranteeing that all item writes complete before readers can observe the new positions. Readers use acquire semantics when loading positions, ensuring they see all writes that happened before the position update. This acquire-release pairing provides the necessary synchronization without requiring expensive locks or full memory barriers.

When a reader's position falls behind the valid range (because the writer has overwritten old data), the reader automatically adjusts by jumping forward to a position slightly ahead of the current first valid item. This underflow handling prevents readers from attempting to access overwritten data while minimizing the chance of immediate re-adjustment.

### Memory Layout and Cache Optimization

The shared memory region begins with a cache-line aligned header containing metadata (version, data size, offset, capacity) and the atomic position value. Data items follow at an offset chosen to maintain cache-line alignment, preventing false sharing between the header and data regions.

Each data item occupies a cache-line aligned slot to ensure that writes to adjacent items don't cause cache coherency traffic. This alignment is critical for performance in multi-core systems where readers and the writer may be running simultaneously on different cores.

The capacity must be a power of two, allowing position-to-index conversion using a fast bitwise AND operation instead of expensive modulo division. The position values continuously increment without wrapping, with overflow being handled correctly by unsigned integer arithmetic semantics.

### Position Encoding and Wrapping

The position encoding packs two 32-bit counters into a single 64-bit atomic value. The lower half contains the first valid position, and the upper half contains the last position (one past the last valid item). This encoding allows atomic updates while preserving 32-bit worth of wrapping space for each counter.

Position values are never reset and naturally wrap around after 2^32 operations. The implementation relies on unsigned integer overflow behavior, where arithmetic operations produce correct results even after wrapping. For example, computing the buffer size as `last - first` yields the correct result regardless of whether either value has wrapped, as long as the actual queue size never exceeds capacity.

### Type Requirements and Constraints

Types stored in the ring buffer must satisfy strict requirements due to the shared memory nature of the implementation. Items must be trivially copyable since they are transferred via memory copy operations, and trivially destructible since no destructors are called when items are overwritten. Additionally, item sizes are limited to the system page size (typically 4096 bytes) to ensure efficient shared memory operations.

The ring buffer capacity must not exceed the maximum value of an unsigned integer and must be a power of two. The actual usable capacity is one less than the specified value to maintain the distinction between empty and full states in the position encoding.

### Iterator Support

The library provides input iterator support through `ring_buffer_iterator<T>`, enabling range-based for loops and standard algorithm usage. The iterator advances the reader's position on increment and blocks on dereference if no items are available, providing a convenient interface while maintaining the underlying lock-free semantics.

### Error Handling and Compatibility

The implementation performs version checking to ensure that readers and writers use compatible buffer layouts. It also validates that the item size matches between the stored type and the reader's type, preventing silent data corruption from type mismatches. These checks occur at reader construction time, failing fast if incompatibilities are detected.

## Configure and build the project

Configure, build and run tests:
```shell
cmake --preset debug
cmake --build build --preset debug
ctest --preset debug
```

Or with a single command:
```shell
cmake --workflow --preset debug
```

## Usage Examples

### 1. Basic Ring Buffer Creation and Item Pushing

```cpp
#include "ringbuffer/ringbuffer.hpp"
#include <string>
#include <iostream>

int main() {
    // Create a ring buffer with capacity for 1024 integers
    // The shared memory object will be automatically removed when the last instance is destroyed
    rb::ring_buffer<int> rb("my_ring_buffer", 1024, true);

    // Push items to the buffer
    for (int i = 0; i < 10; ++i) {
        rb.push(i);
    }

    std::cout << "Buffer size: " << rb.size() << std::endl;
    std::cout << "Buffer capacity: " << rb.capacity() << std::endl;
}
```

### 2. Ring Buffer Reader Creation and Item Retrieval

```cpp
#include "ringbuffer/ringbuffer.hpp"
#include <string>
#include <iostream>

int main() {
    // Create a reader attached to the existing shared memory
    rb::ring_buffer_reader<int> reader("my_ring_buffer");

    // Read items until the buffer is empty
    while (!reader.empty()) {
        int value = reader.get();
        reader.next();
        std::cout << "Read value: " << value << std::endl;
    }
}
```

### 3. Range-based Access Patterns

```cpp
#include "ringbuffer/ringbuffer.hpp"
#include <string>
#include <iostream>
#include <algorithm>

int main() {
    rb::ring_buffer_reader<int> reader("my_ring_buffer");

    for (int val : reader) {
        std::cout << "Value: " << val << std::endl;
    }
}
```

### 4. Concurrent Usage Examples

The ring buffer supports single-producer, multiple-consumer patterns across processes:

**Writer Process:**
```cpp
#include "ringbuffer/ringbuffer.hpp"
#include <thread>
#include <chrono>

int main() {
    rb::ring_buffer<int> rb("concurrent_buffer", 4096, true);

    for (int i = 0; i < 1000; ++i) {
        rb.push(i);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
```

**Reader Process (can be multiple instances):**
```cpp
#include "ringbuffer/ringbuffer.hpp"
#include <iostream>

int main() {
    rb::ring_buffer_reader<int> reader("concurrent_buffer");

    for (int val : reader) {
        std::cout << "Process " << getpid() << " read: " << val << std::endl;
    }
}
```

### 5. Error Handling Patterns

```cpp
#include "ringbuffer/ringbuffer.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        // Attempt to create or open a ring buffer
        rb::ring_buffer<int> rb("nonexistent_buffer", 1024, false);

        // Operations that might fail
        if (rb.empty()) {
            std::cout << "Buffer is empty" << std::endl;
        }
    } catch (std::runtime_error & e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
    } catch (boost::interprocess::interprocess_exception & e) {
        std::cerr << "Interprocess error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
    }
}
```

### 6. Command-line Utility Usage

The project includes a test utility for benchmarking and concurrent testing:

```bash
# Run concurrent test with 2 readers, 64-byte items, 4096-item buffer size
./build/test/test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096

# Use threads instead of processes
./build/test/test_ringbuffer_concur --readers=2 --item-size=64 --rb-size=4096 --use-threads

# Run with custom item count
./build/test/test_ringbuffer_concur --readers=4 --item-size=32 --item-count=1000000

# Show help
./build/test/test_ringbuffer_concur --help
```

### Custom Data Types

The ring buffer supports any trivially copyable and trivially destructible type:

```cpp
#include "ringbuffer/ringbuffer.hpp"

struct SensorData {
    long timestamp;
    double temperature;
    double humidity;
};

// Ensure the type meets requirements
static_assert(std::is_trivially_copyable_v<SensorData>,
              "SensorData must be trivially copyable");
static_assert(std::is_trivially_destructible_v<SensorData>,
              "SensorData must be trivially destructible");

int main() {
    rb::ring_buffer<SensorData> sensorBuffer("sensor_data", 1024, true);

    // Emplace construct items directly in the buffer
    sensorBuffer.emplace(1234567890, 25.5, 45.2);
}
```
