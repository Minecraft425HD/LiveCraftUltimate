#pragma once

#include <cstddef>
#include <cstdint>

namespace lcu {

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;

// Non-copyable mixin for RAII resource owners (windows, GPU handles, job
// system contexts). Move-only by default via the deleted copy ops; derived
// types opt into move semantics explicitly if they need them.
class NonCopyable {
   public:
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;

   protected:
    ~NonCopyable() = default;
};

}  // namespace lcu
