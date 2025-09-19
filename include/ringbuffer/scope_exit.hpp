#pragma once

/// Purpose: provide macro to execute an arbitrary code on scope exit.
///
/// Example:
/// ```
///   FILE * f = fopen("file", "r");
///   scope(exit) { close(f); };
/// ```
///
/// Modern C++23 alternative:
/// ```
///   FILE * f = fopen("file", "r");
///   auto cleanup = std::scope_exit([f] { fclose(f); });
/// ```
///
/// Note:
/// Do not use BOOST_SCOPE_EXIT.  It uses std::function internally and therefore allocates memory.
/// This implementation is conditionally noexcept and provides better exception safety.

#include <utility>

namespace util
{
namespace detail
{

template <typename F>
struct scope_guard
{
    scope_guard(F && f)
        : action{std::forward<F>(f)}
    {}

    ~scope_guard() noexcept(noexcept(std::declval<F &>()())) { action(); }

    scope_guard(const scope_guard &) = delete;
    scope_guard & operator=(const scope_guard &) = delete;

    F action;
};

struct scope_guard_tag
{
    template <typename F>
    friend auto operator+(scope_guard_tag, F && f)
    {
        return scope_guard<F>(std::forward<F>(f));
    }
};

}  // namespace detail
}  // namespace util

#define SCOPE_CONCAT2_(X, Y) X##Y
#define SCOPE_CONCAT_(X, Y)  SCOPE_CONCAT2_(X, Y)

#define scope(condition) scope_##condition
#define scope_exit                                                                                                     \
    auto const & SCOPE_CONCAT_(scope_guard_obj_, __COUNTER__) __attribute__((unused)) =                                \
        util::detail::scope_guard_tag{} + [&]
