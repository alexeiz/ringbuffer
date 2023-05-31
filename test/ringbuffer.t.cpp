#include <Util/Ringbuffer.hpp>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE test_ringbuffer
#include <boost/test/included/unit_test.hpp>

#include <boost/interprocess/managed_shared_memory.hpp>

namespace
{
struct shm_guard
{
    shm_guard(char const * name)
        : name_{name}
    { boost::interprocess::shared_memory_object::remove(name_); }

    ~shm_guard()
    { boost::interprocess::shared_memory_object::remove(name_); }

    char const * name_;
};

std::size_t const  rb_cap  = 4096;
char const * const rb_name = "ring_buffer_test";
}  // anonymous namespace


BOOST_AUTO_TEST_CASE(create_ring_buffer)
{
    shm_guard _{rb_name};
    tsq::ring_buffer<int> rb{rb_name, rb_cap};

    BOOST_CHECK_EQUAL(rb.capacity(), rb_cap);
    BOOST_CHECK_EQUAL(rb.size(), 0);
    BOOST_CHECK(rb.empty());
}

BOOST_AUTO_TEST_CASE(push_items_into_ring_buffer)
{
    std::size_t const cap = 256;
    shm_guard _{rb_name};
    tsq::ring_buffer<char> rb{rb_name, cap};

    for (int i = 0; i != cap; ++i)
    {
        BOOST_CHECK_EQUAL(rb.size(), i);
        rb.push(char(i));
    }

    BOOST_CHECK_EQUAL(rb.capacity(), cap);
    BOOST_CHECK_EQUAL(rb.size(), cap - 1);
    BOOST_CHECK(!rb.empty());
}

struct TestItem
{
    TestItem(int a, double b)
        : a_{a}
        , b_{b}
    {}

    int    a_;
    double b_;
};

BOOST_AUTO_TEST_CASE(emplace_items_into_ring_buffer)
{
    std::size_t const cap = 256;
    shm_guard _{rb_name};
    tsq::ring_buffer<TestItem> rb{rb_name, cap};

    for (int i = 0; i != cap; ++i)
    {
        BOOST_CHECK_EQUAL(rb.size(), i);
        rb.emplace(i, 1.0 + i);
    }

    BOOST_CHECK_EQUAL(rb.capacity(), cap);
    BOOST_CHECK_EQUAL(rb.size(), cap - 1);
    BOOST_CHECK(!rb.empty());
}

BOOST_AUTO_TEST_CASE(create_ring_buffer_reader)
{
    shm_guard _{rb_name};
    tsq::ring_buffer<int> rbw{rb_name, rb_cap};
    tsq::ring_buffer_reader<int> rbr{rb_name};

    BOOST_CHECK_EQUAL(rbr.size(), 0);
    BOOST_CHECK(rbr.empty());
}

BOOST_AUTO_TEST_CASE(get_item_from_read_buffer)
{
    shm_guard _{rb_name};
    tsq::ring_buffer<TestItem> rbw{rb_name, rb_cap};
    tsq::ring_buffer_reader<TestItem> rbr{rb_name};

    rbw.emplace(0x1234abcd, 3.1415926);
    BOOST_CHECK_EQUAL(rbr.size(), 1);

    auto item = rbr.get();
    BOOST_CHECK_EQUAL(item.a_, 0x1234abcd);
    BOOST_CHECK_EQUAL(item.b_, 3.1415926);
}

BOOST_AUTO_TEST_CASE(next_item_in_read_buffer)
{
    shm_guard _{rb_name};
    tsq::ring_buffer<TestItem> rbw{rb_name, rb_cap};
    tsq::ring_buffer_reader<TestItem> rbr{rb_name};

    rbw.emplace(0x1234abcd, 3.1415926);
    BOOST_CHECK_EQUAL(rbr.size(), 1);

    rbr.next();
    BOOST_CHECK_EQUAL(rbr.size(), 0);
    rbr.next();
    BOOST_CHECK(rbr.empty());
}

BOOST_AUTO_TEST_CASE(next_n_items_in_read_buffer)
{
    shm_guard _{rb_name};
    tsq::ring_buffer<int> rbw{rb_name, rb_cap};
    tsq::ring_buffer_reader<int> rbr{rb_name};

    // test writing `count` and jumping over `count`
    int count = 10;
    for (int i = 0; i != count; ++i)
        rbw.push(i);

    BOOST_CHECK_EQUAL(rbr.size(), count);
    rbr.next(count);
    BOOST_CHECK_EQUAL(rbr.size(), 0);

    // test -1 boundary case
    for (int i = 0; i != count; ++i)
        rbw.push(i);

    BOOST_CHECK_EQUAL(rbr.size(), count);
    rbr.next(count - 1);
    BOOST_CHECK_EQUAL(rbr.size(), 1);
    rbr.next();
    BOOST_CHECK_EQUAL(rbr.size(), 0);

    // test +1 boundary case
    for (int i = 0; i != count; ++i)
        rbw.push(i);

    BOOST_CHECK_EQUAL(rbr.size(), count);
    rbr.next(count + 1);
    BOOST_CHECK_EQUAL(rbr.size(), 0);
}

BOOST_AUTO_TEST_CASE(reader_incompatible_with_writer)
{
    auto test_expr = []{
        shm_guard _{rb_name};
        tsq::ring_buffer<TestItem> rbw{rb_name, rb_cap};
        tsq::ring_buffer_reader<int> rbr{rb_name};  // TestItem != int
    };

    auto test_except = [](auto const & e) { return !std::string(e.what()).empty(); };

    BOOST_CHECK_EXCEPTION(
        test_expr(),
        std::runtime_error,
        test_except);
}

BOOST_AUTO_TEST_CASE(interleaved_write_and_read)
{
    shm_guard _{rb_name};
    tsq::ring_buffer<int> rbw{rb_name, rb_cap};
    tsq::ring_buffer_reader<int> rbr{rb_name};

    int write_read_diff = 0;
    for (int i = 0; i != rb_cap * rb_cap; ++i)
    {
        rbw.push(i);
        write_read_diff += rbr.get() - i;
        rbr.next();
    }

    BOOST_CHECK_EQUAL(write_read_diff, 0);
    BOOST_CHECK(rbr.empty());
}

BOOST_AUTO_TEST_CASE(read_after_write_overflow)
{
    shm_guard _{rb_name};
    tsq::ring_buffer<int> rbw{rb_name, rb_cap};
    tsq::ring_buffer_reader<int> rbr{rb_name};

    // get close to overflow
    for (int i = 0; i != rb_cap - 1; ++i)
        rbw.push(i);

    BOOST_CHECK_EQUAL(rbr.get(), 0);
    rbr.next();

    // produce overflow
    rbw.push(rb_cap);
    rbw.push(rb_cap + 1);
    BOOST_CHECK_NE(rbr.get(), 1);  // skipped some data because of overflow
    rbr.next();
    BOOST_CHECK_LE(rbr.size(), rb_cap - 2);

    // push twice as many items as capacity
    for (int i = rb_cap + 2; i != rb_cap + 2 + 2 * rb_cap; ++i)
        rbw.push(i);

    int cur_data = rbr.get();
    rbr.next();
    std::size_t cur_size = rbr.size();
    BOOST_CHECK_LE(cur_size, rb_cap - 2);

    // exhaust all remaining items
    for (std::size_t i = 0; i != cur_size; ++i)
    {
        BOOST_CHECK_EQUAL(rbr.get(), cur_data + i + 1);
        rbr.next();
    }

    BOOST_CHECK_EQUAL(rbr.size(), 0);
}

BOOST_AUTO_TEST_CASE(read_ring_buffer_with_iterator)
{
    shm_guard _{rb_name};
    tsq::ring_buffer<int> rbw{rb_name, rb_cap};
    tsq::ring_buffer_reader<int> rbr{rb_name};

    for (int i = 0; i != rb_cap - 1; ++i)
        rbw.push(i);

    int i = 0;
    for (auto val : rbr)
    {
        BOOST_CHECK_EQUAL(val, i);
        ++i;
    }

    BOOST_CHECK_EQUAL(i, rb_cap - 1);
}
