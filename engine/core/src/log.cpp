#include "lcu/core/log.h"

#include <cstdio>

namespace lcu::log {

const char* level_tag(Level level) {
    switch (level) {
        case Level::Trace:
            return "TRACE";
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO";
        case Level::Warn:
            return "WARN";
        case Level::Error:
            return "ERROR";
    }
    return "?????";
}

void write_line(Level level, const std::string& message) {
    std::FILE* stream = (level == Level::Warn || level == Level::Error) ? stderr : stdout;
    fmt::print(stream, "[{}] {}\n", level_tag(level), message);
}

}  // namespace lcu::log
