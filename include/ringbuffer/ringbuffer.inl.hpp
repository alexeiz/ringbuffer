#pragma once

#include "ringbuffer.hpp"

#include <algorithm>
#include <limits>
#include <unistd.h>
#include <stdexcept>
#include <utility>
#include <new>

namespace rb
{
// ring_buffer implementation

template <ring_buffer_value T>
ring_buffer<T>::ring_buffer(std::string_view name, std::size_t capacity, bool remove_on_close)
    : capacity_(capacity)
{
    if (capacity_ == 0 || capacity_ > std::numeric_limits<unsigned>::max())
        throw std::invalid_argument("ring_buffer capacity out of valid range [1, max(unsigned int)]");

    if ((capacity_ & (capacity_ - 1)) != 0)
        throw std::invalid_argument("ring_buffer capacity should be a power of 2");

    // data alignment relies on the statically defined cache line size being no less than the number reported
    // by the OS
    if (std::cmp_greater(sysconf(_SC_LEVEL1_DCACHE_LINESIZE), detail::ring_buffer_cache_linesize))
        throw std::runtime_error("system cache line size is not equal to the expected value");

    std::size_t page_size(sysconf(_SC_PAGESIZE));  // shared memory is aligned on system page size
    if (sizeof(data_t) > page_size)
        throw std::runtime_error("ring_buffer cannot store objects larger than the system page size");

    std::size_t data_offset = std::max(sizeof(header_t), sizeof(data_t));
    data_offset = ((data_offset - 1) / sizeof(data_t) + 1) * sizeof(data_t);  // align the data offset

    std::size_t store_size = capacity_ * sizeof(data_t) + data_offset;

    // initialize data fields
    store_ = std::make_shared<ring_buffer_store>(ring_buffer_store::create, name, store_size, remove_on_close);
    header_ = new (store_->address()) header_t{detail::ring_buffer_version, sizeof(T), data_offset, capacity_};
    data_ = reinterpret_cast<data_t *>(static_cast<char *>(store_->address()) + data_offset);
}

template <ring_buffer_value T>
inline void ring_buffer<T>::push(T const & val)
{
    push_helper([&val](data_t * where) { new (where) T(val); });
}

template <ring_buffer_value T>
template <typename... Args>
inline void ring_buffer<T>::emplace(Args &&... args)
{
    push_helper([&args...](data_t * where) { new (where) T{std::forward<Args>(args)...}; });
}

template <ring_buffer_value T>
template <typename Init>
void ring_buffer<T>::push_helper(Init init)
{
    [[assume(capacity_ > 0)]];
    [[assume((capacity_ & (capacity_ - 1)) == 0)]];  // capacity_ is power of 2

    // get current positions
    auto pos = header_->positions.load(std::memory_order_relaxed);
    unsigned first = header_t::first(pos);
    unsigned last = header_t::last(pos);

    // place the item
    init(&data_[last & (capacity_ - 1)]);  // [first, last) range goes beyond [0, capacity), so it needs
                                           // adjustment, since capacity_ is a power of 2, (last & (capacity_ - 1))
                                           // is equivalent to (last % capacity_)

    // compute new positions
    ++last;
    if (first + capacity_ - 1 < last)  // capacity-1 to avoid overlapping reader and writer
        first = last - capacity_ + 1;

    // update positons with memory_release
    header_->positions.store(header_t::make_positions(first, last), std::memory_order_release);
}

template <ring_buffer_value T>
inline std::size_t ring_buffer<T>::size() const
{
    auto pos = header_->positions.load(std::memory_order_relaxed);
    return header_t::last(pos) - header_t::first(pos);  // correct even after overflow
}

// ring_buffer_reader implementation

template <ring_buffer_value T>
ring_buffer_reader<T>::ring_buffer_reader(std::string_view name, unsigned underflow_fixup)
    : store_{std::make_shared<ring_buffer_store>(ring_buffer_store::open, name)}
    , underflow_fixup_{underflow_fixup}
    , read_pos_{0}
    , header_{static_cast<header_t *>(store_->address())}
    , data_{reinterpret_cast<data_t *>(static_cast<char *>(store_->address()) + header_->data_offset)}
    , capacity_mask_{header_->capacity - 1}  // avoid touching the header later without a good reason
{
    // verify version
    if (header_->version != detail::ring_buffer_version)
        throw std::runtime_error("ring_buffer stored version incompatible with this implementation");

    // verify data item size
    if (header_->data_size != sizeof(T))
        throw std::runtime_error("ring_buffer stored data item size incompatible with reader data item");

    [[assume(header_->version == detail::ring_buffer_version)]];
    [[assume(header_->data_size == sizeof(T))]];

    // initialize positions
    auto pos = header_->positions.load(std::memory_order_acquire);
    read_pos_ = header_t::first(pos);
}

template <ring_buffer_value T>
inline std::size_t ring_buffer_reader<T>::size() const
{
    auto pos = header_->positions.load(std::memory_order_acquire);
    adjust_read_pos(pos);

    auto last = header_t::last(pos);
    return last > read_pos_ ? last - read_pos_ : 0;
}

template <ring_buffer_value T>
inline void ring_buffer_reader<T>::adjust_read_pos(unsigned long pos) const
{
    unsigned first = header_t::first(pos);
    if (first > read_pos_)
    {
        // [first, last) range has moved beyond the current read position
        // point the reader position to the new first item position
        read_pos_ = first + underflow_fixup_;
    }
}

template <ring_buffer_value T>
inline void ring_buffer_reader<T>::spin_wait(unsigned long pos) const
{
    while (read_pos_ >= header_t::last(pos))
    {
        pos = header_->positions.load(std::memory_order_acquire);
        adjust_read_pos(pos);
    }
}

template <ring_buffer_value T>
T ring_buffer_reader<T>::get() const
{
    [[assume(capacity_mask_ >= 0)]];

    auto pos = header_->positions.load(std::memory_order_acquire);
    adjust_read_pos(pos);
    spin_wait(pos);

    for (;;)
    {
        T item(data_[read_pos_ & capacity_mask_].item);

        // re-read positions to see if the current read position is still valid
        unsigned last_read_pos = read_pos_;
        pos = header_->positions.load(std::memory_order_acquire);
        adjust_read_pos(pos);

        if (last_read_pos == read_pos_)
            return item;  // read position and the item didn't get invalidated
    }
}

template <ring_buffer_value T>
inline void ring_buffer_reader<T>::next(std::size_t n)
{
    read_pos_ += n;
}

template <ring_buffer_value T>
inline ring_buffer_iterator<T> ring_buffer_reader<T>::begin()
{
    return ring_buffer_iterator<T>{*this};
}

template <ring_buffer_value T>
inline ring_buffer_iterator<T> ring_buffer_reader<T>::end()
{
    return {};
}

}  // namespace rb
