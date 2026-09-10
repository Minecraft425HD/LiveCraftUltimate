#include "lcu/core/assert.h"

namespace lcu::detail {

[[noreturn]] void assert_fail(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "Assertion failed: %s\n  at %s:%d\n", expr, file, line);
    std::fflush(stderr);
    std::abort();
}

}  // namespace lcu::detail
