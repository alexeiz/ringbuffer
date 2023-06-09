/// Purpose: provide IPC via fixed-size lock-free ring buffer

#pragma once

#include "ringbufferstore.hpp"

#include <boost/iterator/iterator_facade.hpp>

#include <cstddef>
#include <string>
#include <memory>
#include <atomic>
#include <type_traits>
#include <utility>
#include <stdexcept>

namespace tsq
{
namespace detail
{
// constants
static constexpr int         ring_buffer_version        = 1;
static constexpr std::size_t ring_buffer_cache_linesize = 64;

// ring buffer header information
struct ring_buffer_header
{
    ring_buffer_header(int v, std::size_t ds, std::size_t offset, std::size_t cap)
        : version{v}
        , data_size{ds}
        , data_offset{offset}
        , capacity{cap}
        , positions{0}
    {}

    union position_t
    {
        unsigned long lpos;
        unsigned      upos[2];

        position_t(unsigned long p)        : lpos{p}    {}
        position_t(unsigned f, unsigned s) : upos{f, s} {}

        static_assert(sizeof(unsigned long) >= 2 * sizeof(unsigned),
                      "two counters should fit into one long value");
    };

    static unsigned first(unsigned long pos)
    {
        position_t p = {pos};
        return p.upos[0];
    }

    static unsigned last(unsigned long pos)
    {
        position_t p = {pos};
        return p.upos[1];
    }

    static unsigned long make_positions(unsigned first, unsigned last)
    {
        position_t p = {first, last};
        return p.lpos;
    }

    int                        version;     ///< version of the ring buffer to ensure reader/write compatibility
    std::size_t                data_size;   ///< size (in bytes) of data items inside the ring buffer
    std::size_t                data_offset; ///< offset at which data items start
    std::size_t                capacity;    ///< maximum number of items the ring buffer can contain
    std::atomic<unsigned long> positions;   ///< combined first/last element positions
};

// ring buffer data item
template <typename T>
struct ring_buffer_data
{
    union
    {
        std::aligned_storage_t<sizeof(T), ring_buffer_cache_linesize> alignement;
        T                                                             item;
    };
};

}  // namespace detail

/// The `ring_buffer` class provides a way of interprocess communication via lock-free fixed size
/// ring-buffer/queue.  It's a single-producer/multiple-consumer queue, where consumers do not remove items
/// from the queue (effectively consumers only observe the queue).  The queue does not provide a way to know
/// how many consumers it has.
///
/// \b Requirements:
///  - T must be 'trivially destructible'
template <typename T>
class ring_buffer
{
    // requirements
    static_assert(std::is_trivially_destructible<T>::value, "T's dtor must be trivial");

    // types
    using header_t = detail::ring_buffer_header;
    using data_t = detail::ring_buffer_data<T>;

public:
    /// Construct the ring buffer in a shared memory.
    ///
    /// \param name             shared memory object name
    /// \param capacity         maximum capacity of the ring buffer (number of items)
    /// \param remove_on_close  should the shared memory object file be removed when the ring buffer is destructed
    /// \throws boost::interprocess::interprocess_exception if mapping a shared memory region fails
    ring_buffer(char const * name, std::size_t capacity, bool remove_on_close = false);

    /// Push `val` to the end of the queue.
    void push(T const & val);

    /// Emplace an item to the end of the queue by constructing it with `args` arguments.
    template <typename ...Args>
    void emplace(Args && ... args);

    /// \returns capacity of the ring buffer
    std::size_t capacity() const { return capacity_; }

    /// \returns size (number of items) of the ring buffer
    std::size_t size() const;

    /// \returns `true` if the ring buffer is empty (`size() == 0`)
    bool empty() const { return size() == 0; }

private:
    /// Push item with generic in-place item initializer.
    ///
    /// \b Requirements:
    ///  - Init must be callable as function of type void(data *)
    template <typename Init>
    void push_helper(Init init);

private:
    std::shared_ptr<ring_buffer_store> store_;    ///< shared memory backing store for the ring buffer
    std::size_t                        capacity_; ///< capacity of the ring buffer (maximum possible number of items)
    header_t *                         header_;   ///< ring buffer header infomation (stored in `store_`)
    data_t *                           data_;     ///< actual items (stored in `store_`)
};


// forward decl
template <typename T> class ring_buffer_iterator;


/// The `ring_buffer_reader` class is the reader for the ring buffer in shared memory.  It operates on the
/// same shared memory objects that `ring_buffer` class writes.  Together with `ring_buffer` these two
/// classes can be used for inter-process communication.  There can be any number of readers for the same
/// shared memory ring buffer.  Readers operate completely independently of each other.
template <typename T>
class ring_buffer_reader
{
    // types
    using header_t = detail::ring_buffer_header;
    using data_t = detail::ring_buffer_data<T>;

public:
    /// Construct the ring buffer reader attached to the `shm_name` shared memory object.
    ///
    /// \param name
    /// \throws std::runtime_exception if mapping a shared memory region fails
    ring_buffer_reader(char const * name);

    /// \returns number of items in the ring buffer available to read by this reader
    std::size_t size() const;

    /// \returns `true` if no items are available to read (`size() == 0`)
    bool empty() const { return size() == 0; }

    /// Get the current item.
    ///
    /// \note blocks in the spinning loop if no items are available
    T get() const;

    /// Advances read position to `n` items forward.
    ///
    /// \note does not block even if not enough items are available
    void next(std::size_t n = 1);

    /// \returns a single-pass (i.e. input) iterator to the ring_buffer allowing to use the ring buffer
    ///          reader with iterator-based loops and algorithms
    ///
    /// \note this iterator reaches the `end` when the reader `empty()` return `true`
    ring_buffer_iterator<T> begin();

    /// \returns an `end` sentinel iterator
    ring_buffer_iterator<T> end();

private:
    void adjust_read_pos(unsigned long pos) const;
    void spin_wait(unsigned long pos) const;

private:
    static constexpr unsigned underflow_fixup_ = 128;  ///< number of items to jump over on read_pos underflow

    std::shared_ptr<ring_buffer_store> store_;         ///< shared memory backing store for the ring buffer
    unsigned mutable                   read_pos_;      ///< position of the next item to read
    header_t const *                   header_;        ///< ring buffer header infomation (stored in `store_`)
    data_t const *                     data_;          ///< actual items (stored in `store_`)
    std::size_t                        capacity_mask_; ///< copy of header_->capacity-1 to avoid touching the header memory
};


/// Class `ring_buffer_iterator` provides a single-pass (input) iteration interface to `ring_buffer_reader`,
/// allowing to use `ring_buffer_reader` with iterator-based loops and algorithms.  The behavior of this
/// iterator is similar to `std::istream_itreator`.
template <typename T>
class ring_buffer_iterator : public boost::iterator_facade<ring_buffer_iterator<T>,
                                                           T const,
                                                           boost::single_pass_traversal_tag>
{
    friend class boost::iterator_core_access;

public:
    /// Create the `end` iterator.
    ring_buffer_iterator()
        : reader_{nullptr}
    {}

    /// Create iterator attached to the specified ring buffer `reader`.
    explicit
    ring_buffer_iterator(ring_buffer_reader<T> & reader)
        : reader_{&reader}
    {}

private:
    // iterator_facade interface
    bool equal(ring_buffer_iterator const & other) const
    {
        if (reader_ && other.reader_)
            return reader_ == other.reader_;
        if (reader_ && !other.reader_)
            return reader_->empty();
        if (!reader_ && other.reader_)
            return other.reader_->empty();
        return true;
    }

    void increment()
    {
        reader_->next();
    }

    T const & dereference() const
    {
        return value_ = reader_->get();
    }

private:
    ring_buffer_reader<T> * reader_;
    T mutable               value_;
};

}  // namespace tsq

#include "ringbuffer.inl.hpp"
