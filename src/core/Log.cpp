#include "core/Log.h"

#include <cstdio>

namespace ee::log {

void write(Level level, const char* fmt, ...) {
    static const char* const names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    std::FILE* out = (level >= Level::Warn) ? stderr : stdout;
    std::fprintf(out, "[%s] ", names[static_cast<int>(level)]);
    std::va_list args;
    va_start(args, fmt);
    std::vfprintf(out, fmt, args);
    va_end(args);
    std::fputc('\n', out);
}

} // namespace ee::log
