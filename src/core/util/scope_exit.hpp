#pragma once

#include <type_traits>
#include <utility>

namespace ripper::pdf::core
{
/// Minimal RAII guard that invokes a callable when it leaves scope.
///
/// Equivalent to `std::scope_exit` from the C++23 `<scope>` header, which is not
/// yet provided by every supported standard library implementation. The callable
/// runs on normal return and while unwinding, unless the guard is released.
/// Once `<scope>` is available on all supported toolchains this header and its
/// uses can be replaced with `std::scope_exit`.
template <typename Callback>
class scope_exit
{
public:
    explicit scope_exit(Callback callback) noexcept(std::is_nothrow_move_constructible_v<Callback>)
        : callback_{std::move(callback)}
    {
    }

    scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;

    scope_exit(scope_exit&& other) noexcept(std::is_nothrow_move_constructible_v<Callback>)
        : callback_{std::move(other.callback_)}, active_{std::exchange(other.active_, false)}
    {
    }

    scope_exit& operator=(scope_exit&&) = delete;

    ~scope_exit()
    {
        if (active_)
            callback_();
    }

    /// Disarm the guard so the callable is not invoked on destruction.
    void release() noexcept
    {
        active_ = false;
    }

private:
    Callback callback_;
    bool active_{true};
};
} // namespace ripper::pdf::core
