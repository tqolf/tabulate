#pragma once

#include <stddef.h>
#include <stdint.h>

// https://graphics.stanford.edu/~seander/bithacks.html

namespace bithacks
{
// Round up to the next highest power of 2
template <typename T>
constexpr T or_equal(T x, unsigned u) noexcept
{
    return x | x >> u;
}

template <typename T, typename... Args>
constexpr T or_equal(T x, unsigned u, Args... rest) noexcept
{
    return or_equal(or_equal(x, u), rest...);
}

constexpr uint32_t round_up_to_power_of_2(uint32_t a) noexcept
{
    return or_equal(a - 1, 1, 2, 4, 8, 16) + 1;
}

constexpr uint64_t round_up_to_power_of_2(uint64_t a) noexcept
{
    return or_equal(a - 1, 1, 2, 4, 8, 16, 32) + 1;
}
} // namespace bithacks
