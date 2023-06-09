#include "ringbufferstore.hpp"

#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE test_ringbufferstore
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

std::size_t const  shm_size = 4096;
char const * const shm_file = "ring_buffer_store_test";
}  // anonymous namespace


// test the creation of ring buffer store object
BOOST_AUTO_TEST_CASE(create_ring_buffer_store)
{
    shm_guard _{shm_file};
    tsq::ring_buffer_store rbs{tsq::ring_buffer_store::create, shm_file, shm_size};

    BOOST_CHECK(rbs.address() != nullptr);
    BOOST_CHECK_EQUAL(rbs.size(), shm_size);
    BOOST_CHECK_EQUAL(rbs.mode(), tsq::ipc::read_write);
}

// test opening an existing ring buffer store
BOOST_AUTO_TEST_CASE(open_ring_buffer_store)
{
    shm_guard _{shm_file};
    tsq::ring_buffer_store rbs_create{tsq::ring_buffer_store::create, shm_file, shm_size};
    tsq::ring_buffer_store rbs{tsq::ring_buffer_store::open, shm_file};

    BOOST_CHECK(rbs.address() != nullptr);
    BOOST_CHECK_EQUAL(rbs.size(), shm_size);
    BOOST_CHECK_EQUAL(rbs.mode(), tsq::ipc::read_only);
}

// test writing and reading from read buffer store
BOOST_AUTO_TEST_CASE(read_write_ring_buffer_store)
{
    shm_guard _{shm_file};
    tsq::ring_buffer_store rbs_create{tsq::ring_buffer_store::create, shm_file, shm_size};
    tsq::ring_buffer_store rbs{tsq::ring_buffer_store::open, shm_file};

    char * write_ptr = static_cast<char *>(rbs_create.address());
    for (std::size_t i = 0; i != shm_size; ++i)
        write_ptr[i] = char(i);

    char read_write_diff = 0;
    char const * read_ptr = static_cast<char const *>(rbs.address());
    for (std::size_t i = 0; i != shm_size; ++i)
        read_write_diff += read_ptr[i] - write_ptr[i];

    BOOST_CHECK_EQUAL(read_write_diff, 0);
}

BOOST_AUTO_TEST_CASE(fail_create_ring_buffer_store)
{
    auto test_expr = []{ tsq::ring_buffer_store rbs{tsq::ring_buffer_store::create, "///", 64}; };
    auto test_except = [](auto const & e) { return e.get_error_code() == boost::interprocess::invalid_argument; };

    BOOST_CHECK_EXCEPTION(
        test_expr(),
        boost::interprocess::interprocess_exception,
        test_except);
}

BOOST_AUTO_TEST_CASE(fail_open_ring_buffer_store)
{
    auto test_expr = []{ tsq::ring_buffer_store rbs{tsq::ring_buffer_store::open, shm_file}; };
    auto test_except = [](auto const & e) { return e.get_error_code() == boost::interprocess::not_found_error; };

    BOOST_CHECK_EXCEPTION(
        test_expr(),
        boost::interprocess::interprocess_exception,
        test_except);
}
