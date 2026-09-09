#pragma once

// Assertion macros. LCU_ASSERT is compiled out (condition not even
// evaluated) in Release builds without LCU_DEVELOPMENT_BUILD defined.
// LCU_VERIFY always evaluates its condition and always aborts on failure,
// in every build configuration — use it when the condition has required
// side effects or must never silently pass. Both print via stderr directly
// rather than through lcu::log, to avoid a hard dependency from
// core/assert.h on core/log.h's fmt-based formatting for a code path that
// must work even if logging itself is broken.

#include <cstdio>
#include <cstdlib>

namespace lcu::detail {

[[noreturn]] void assert_fail(const char* expr, const char* file, int line);

}  // namespace lcu::detail

#if defined(NDEBUG) && !defined(LCU_DEVELOPMENT_BUILD)
#define LCU_ASSERT(cond) ((void)0)
#else
#define LCU_ASSERT(cond)                                     \
    do {                                                     \
        if (!(cond)) {                                       \
            ::lcu::detail::assert_fail(#cond, __FILE__, __LINE__); \
        }                                                     \
    } while (0)
#endif

// Always evaluates `cond`; aborts on failure outside NDEBUG-only Release
// builds. Use for conditions with required side effects.
#define LCU_VERIFY(cond)                                          \
    do {                                                          \
        if (!(cond)) {                                            \
            ::lcu::detail::assert_fail(#cond, __FILE__, __LINE__); \
        }                                                          \
    } while (0)
