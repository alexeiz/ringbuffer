/// Purpose: private details of the ring buffer storage implementation

#pragma once

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

namespace tla
{
namespace ipc = boost::interprocess;

namespace detail
{
// Private wrapper for boost::interprocess::shared_memory_object that simplifies initialization.
struct shm_object_holder
{
    // Create and size a shared memory object.
    shm_object_holder(char const * shm_name,
                      std::size_t  size,
                      ipc::mode_t  mode,
                      bool         remove = false)
        : obj_(ipc::create_only, shm_name, mode)
        , remove_on_close_(remove)
    {
        obj_.truncate(size);
    }

    // Open an existing shared memory object.
    shm_object_holder(char const * shm_name,
                      ipc::mode_t  mode)
        : obj_(ipc::open_only, shm_name, mode)
    {}

    ~shm_object_holder()
    {
        if (remove_on_close_)
            ipc::shared_memory_object::remove(obj_.get_name());
    }

    ipc::shared_memory_object obj_;
    bool                      remove_on_close_{false};

};
}  // namespace detail

class ring_buffer_store
{
public:
    class create_t {};
    class open_t {};

    static constexpr create_t create{};
    static constexpr open_t   open{};

public:
    ring_buffer_store(ring_buffer_store const &)             = delete;
    ring_buffer_store & operator=(ring_buffer_store const &) = delete;

    /// Construct a ring buffer in a newly created shared memory object.
    ///
    /// \note size in bytes
    ring_buffer_store(create_t, char const * shm_name, std::size_t size, bool remove_on_close = false)
        : shm_(shm_name, size, ipc::read_write)
        , region_(shm_.obj_, ipc::read_write, 0, 0, nullptr)
        , mode_(ipc::read_write)
    {}

    /// Open an existing shared memory object and read ring buffer information from it.
    ring_buffer_store(open_t, char const * shm_name)
        : shm_(shm_name, ipc::read_only)
        , region_(shm_.obj_, ipc::read_only, 0, 0, nullptr)
        , mode_(ipc::read_only)
    {}

    /// \returns start address of the shared memory object
    void * address() const
    {
        return region_.get_address();
    }

    /// \return size of the shared memory object in bytes
    std::size_t size() const
    {
        return region_.get_size();
    }

    /// \return read/write mode of the shared memory object
    ipc::mode_t mode() const
    {
        return mode_;
    }

private:
    detail::shm_object_holder shm_;
    ipc::mapped_region        region_;
    ipc::mode_t               mode_;
};

}  // namespace tla


