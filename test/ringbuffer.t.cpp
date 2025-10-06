#include "ringbuffer/ringbuffer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>

using namespace rb;

namespace
{
struct shm_guard
{
    shm_guard(char const * name)
        : name_{name}
    {
        boost::interprocess::shared_memory_object::remove(name_);
    }

    shm_guard(shm_guard const &) = delete;
    shm_guard & operator=(shm_guard const &) = delete;
    shm_guard(shm_guard &&) = delete;
    shm_guard & operator=(shm_guard &&) = delete;

    ~shm_guard() { boost::interprocess::shared_memory_object::remove(name_); }

    char const * name_;
};

std::size_t const rb_cap = 4096;
char const * const rb_name = "ring_buffer_test";
}  // anonymous namespace


TEST_CASE("create_ring_buffer", "[ringbuffer]")
{
    shm_guard _{rb_name};
    ring_buffer<int> rb{rb_name, rb_cap};

    REQUIRE(rb.capacity() == rb_cap);
    REQUIRE(rb.size() == 0);
    REQUIRE(rb.empty());
}

TEST_CASE("push_items_into_ring_buffer", "[ringbuffer]")
{
    std::size_t const cap = 256;
    shm_guard _{rb_name};
    ring_buffer<char> rb{rb_name, cap};

    for (int i = 0; std::cmp_not_equal(i, cap); ++i)
    {
        REQUIRE(std::cmp_equal(rb.size(), i));
        rb.push(char(i));
    }

    REQUIRE(rb.capacity() == cap);
    REQUIRE(rb.size() == cap - 1);
    REQUIRE(!rb.empty());
}

struct TestItem
{
    TestItem(int a, double b)
        : a_{a}
        , b_{b}
    {}

    int a_;
    double b_;
};

TEST_CASE("emplace_items_into_ring_buffer", "[ringbufer]")
{
    std::size_t const cap = 256;
    shm_guard _{rb_name};
    ring_buffer<TestItem> rb{rb_name, cap};

    for (int i = 0; std::cmp_not_equal(i, cap); ++i)
    {
        REQUIRE(std::cmp_equal(rb.size(), i));
        rb.emplace(i, 1.0 + i);
    }

    REQUIRE(rb.capacity() == cap);
    REQUIRE(rb.size() == cap - 1);
    REQUIRE(!rb.empty());
}

TEST_CASE("create_ring_buffer_reader", "[ringbuffer]")
{
    shm_guard _{rb_name};
    ring_buffer<int> rbw{rb_name, rb_cap};
    ring_buffer_reader<int> rbr{rb_name};

    REQUIRE(rbr.size() == 0);
    REQUIRE(rbr.empty());
}

TEST_CASE("get_item_from_read_buffer", "[ringbuffer]")
{
    shm_guard _{rb_name};
    ring_buffer<TestItem> rbw{rb_name, rb_cap};
    ring_buffer_reader<TestItem> rbr{rb_name};

    rbw.emplace(0x1234abcd, 3.7142);
    REQUIRE(rbr.size() == 1);

    auto item = rbr.get();
    REQUIRE(item.a_ == 0x1234abcd);
    REQUIRE(item.b_ == 3.7142);
}

TEST_CASE("next_item_in_read_buffer", "[ringbuffer]")
{
    shm_guard _{rb_name};
    ring_buffer<TestItem> rbw{rb_name, rb_cap};
    ring_buffer_reader<TestItem> rbr{rb_name};

    rbw.emplace(0x1234abcd, 6.1415);
    REQUIRE(rbr.size() == 1);

    rbr.next();
    REQUIRE(rbr.size() == 0);
    rbr.next();
    REQUIRE(rbr.empty());
}

TEST_CASE("next_n_items_in_read_buffer", "[ringbuffer]")
{
    shm_guard _{rb_name};
    ring_buffer<int> rbw{rb_name, rb_cap};
    ring_buffer_reader<int> rbr{rb_name};

    // test writing `count` and jumping over `count`
    int count = 10;
    for (int i = 0; i != count; ++i)
        rbw.push(i);

    REQUIRE(std::cmp_equal(rbr.size(), count));
    rbr.next(count);
    REQUIRE(rbr.size() == 0);

    // test -1 boundary case
    for (int i = 0; i != count; ++i)
        rbw.push(i);

    REQUIRE(std::cmp_equal(rbr.size(), count));
    rbr.next(count - 1);
    REQUIRE(rbr.size() == 1);
    rbr.next();
    REQUIRE(rbr.size() == 0);

    // test +1 boundary case
    for (int i = 0; i != count; ++i)
        rbw.push(i);

    REQUIRE(std::cmp_equal(rbr.size(), count));
    rbr.next(count + 1);
    REQUIRE(rbr.size() == 0);
}

TEST_CASE("reader_incompatible_with_writer", "[ringbuffer]")
{
    auto test_expr = [] {
        shm_guard _{rb_name};
        ring_buffer<TestItem> rbw{rb_name, rb_cap};
        ring_buffer_reader<int> rbr{rb_name};  // TestItem != int
    };

    REQUIRE_THROWS_AS(test_expr(), std::runtime_error);
}

TEST_CASE("interleaved_write_and_read", "[ringbuffer]")
{
    shm_guard _{rb_name};
    ring_buffer<int> rbw{rb_name, rb_cap};
    ring_buffer_reader<int> rbr{rb_name};

    int write_read_diff = 0;
    for (int i = 0; i != rb_cap * rb_cap; ++i)
    {
        rbw.push(i);
        write_read_diff += rbr.get() - i;
        rbr.next();
    }

    REQUIRE(write_read_diff == 0);
    REQUIRE(rbr.empty());
}

TEST_CASE("read_after_write_overflow", "[ringbuffer]")
{
    shm_guard _{rb_name};
    ring_buffer<int> rbw{rb_name, rb_cap};
    ring_buffer_reader<int> rbr{rb_name};

    // get close to overflow
    for (int i = 0; i != rb_cap - 1; ++i)
        rbw.push(i);

    REQUIRE(rbr.get() == 0);
    rbr.next();

    // produce overflow
    rbw.push(rb_cap);
    rbw.push(rb_cap + 1);
    REQUIRE(rbr.get() != 1);  // skipped some data because of overflow
    rbr.next();
    REQUIRE(rbr.size() <= rb_cap - 2);

    // push twice as many items as capacity
    for (int i = rb_cap + 2; i != rb_cap + 2 + 2 * rb_cap; ++i)
        rbw.push(i);

    int cur_data = rbr.get();
    rbr.next();
    std::size_t cur_size = rbr.size();
    REQUIRE(cur_size <= rb_cap - 2);

    // exhaust all remaining items
    for (std::size_t i = 0; i != cur_size; ++i)
    {
        REQUIRE(std::cmp_equal(rbr.get(), cur_data + i + 1));
        rbr.next();
    }

    REQUIRE(rbr.size() == 0);
}

TEST_CASE("read_ring_buffer_with_iterator", "[ringbuffer]")
{
    shm_guard _{rb_name};
    ring_buffer<int> rbw{rb_name, rb_cap};
    ring_buffer_reader<int> rbr{rb_name};

    for (int i = 0; i != rb_cap - 1; ++i)
        rbw.push(i);

    int i = 0;
    for (auto val : rbr)
    {
        REQUIRE(val == i);
        ++i;
    }

    REQUIRE(i == rb_cap - 1);
}
