# ringbuffer

## Configure and build the project

Install dependencies:
```shell
conan install . --output-folder=build --build=missing -s build_type=Debug
```

Add the **default** preset to the generated **CMakeUserPresets.json** on the same level as `"include"`:
```json
    "configurePresets": [
        {
            "name": "default",
            "inherits": "conan-debug",
            "cacheVariables": {
                "CMAKE_EXPORT_COMPILE_COMMANDS": {
                    "type": "BOOL",
                    "value": "TRUE"
                }
            }
        }
    ],
    "buildPresets": [
        {
            "name": "default",
            "configurePreset": "default"
        }
    ],
    "testPresets": [
        {
            "name": "default",
            "configurePreset": "default"
        }
    ]
```

Configure and build:
```shell
cmake --preset default .
cmake --build build
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
