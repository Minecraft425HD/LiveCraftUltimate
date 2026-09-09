#pragma once

// Minimal logging wrapper around fmt. Call sites use LCU_LOG_* macros
// rather than fmt directly so the backend (currently fmt::print to
// stdout/stderr) can be swapped for spdlog later without touching callers
// (see DECISIONS.md).

#include <fmt/core.h>

namespace lcu::log {

enum class Level { Trace, Debug, Info, Warn, Error };

// Returns the string tag used in log line prefixes, e.g. "INFO".
const char* level_tag(Level level);

// Writes a single formatted log line to stdout (Trace/Debug/Info) or
// stderr (Warn/Error), prefixed with the level tag. Not thread-safe on its
// own; callers doing multi-threaded logging (job system, server) are
// responsible for serializing calls until a proper sink exists.
void write_line(Level level, const std::string& message);

}  // namespace lcu::log

#define LCU_LOG_TRACE(...) \
    ::lcu::log::write_line(::lcu::log::Level::Trace, ::fmt::format(__VA_ARGS__))
#define LCU_LOG_DEBUG(...) \
    ::lcu::log::write_line(::lcu::log::Level::Debug, ::fmt::format(__VA_ARGS__))
#define LCU_LOG_INFO(...) \
    ::lcu::log::write_line(::lcu::log::Level::Info, ::fmt::format(__VA_ARGS__))
#define LCU_LOG_WARN(...) \
    ::lcu::log::write_line(::lcu::log::Level::Warn, ::fmt::format(__VA_ARGS__))
#define LCU_LOG_ERROR(...) \
    ::lcu::log::write_line(::lcu::log::Level::Error, ::fmt::format(__VA_ARGS__))
