#include <catch2/catch_test_macros.hpp>
#include "ringbuffer/ringbuffer.hpp"
#include <coroutine>
#include <string>

using namespace rb;

struct task {
    struct promise_type {
        task get_return_object() { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
    std::coroutine_handle<promise_type> h;
};

task my_coro(async_ring_buffer_reader<int>& reader, int& out_val) {
    out_val = co_await reader;
    reader.next();
    out_val = co_await reader;
    reader.next();
}

TEST_CASE("async_ring_buffer_reader co_await tests", "[async][coroutine]") {
    ring_buffer<int> rb("test_async_rb", 16, true);
    async_ring_buffer_reader<int> reader("test_async_rb");

    int out_val = 0;
    auto t = my_coro(reader, out_val);

    // start coroutine
    t.h.resume();

    REQUIRE(out_val == 0); // Should yield because it's empty

    // push an item
    rb.push(42);

    // resume coroutine, now it should read
    t.h.resume();

    REQUIRE(out_val == 42); // Should have read 42

    // the coroutine should be suspended waiting for the second item
    rb.push(100);

    t.h.resume();

    REQUIRE(out_val == 100);

    t.h.destroy();
}

task test_yield(async_ring_buffer_reader<int>& reader, int& yield_count) {
    while (true) {
        int val = co_await reader;
        reader.next();
        yield_count++;
        if (val == -1) break;
    }
}

TEST_CASE("async_ring_buffer_reader multiple yields", "[async][coroutine]") {
    ring_buffer<int> rb("test_async_yield_rb2", 16, true);
    async_ring_buffer_reader<int> reader("test_async_yield_rb2");

    int yield_count = 0;
    auto t = test_yield(reader, yield_count);

    t.h.resume();
    REQUIRE(yield_count == 0); // Suspended

    rb.push(1);
    t.h.resume();
    REQUIRE(yield_count == 1);

    // Check consecutive pushing
    rb.push(2);
    rb.push(3);

    t.h.resume(); // resumes reading 2. BUT the while loop loops immediately and reads 3 as well without suspending!
    // Since we use the same thread, both 2 and 3 are read because await_ready() returns true for 3.
    REQUIRE(yield_count == 3);

    // Now it's empty, and should be suspended inside co_await
    REQUIRE(reader.empty());

    rb.push(-1); // Stop
    t.h.resume();
    REQUIRE(yield_count == 4);

    t.h.destroy();
}
