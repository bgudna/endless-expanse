#pragma once

#include <cstdarg>

namespace ee::log {

enum class Level { Debug, Info, Warn, Error };

void write(Level level, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

} // namespace ee::log

#define EE_LOG_DEBUG(...) ::ee::log::write(::ee::log::Level::Debug, __VA_ARGS__)
#define EE_LOG_INFO(...) ::ee::log::write(::ee::log::Level::Info, __VA_ARGS__)
#define EE_LOG_WARN(...) ::ee::log::write(::ee::log::Level::Warn, __VA_ARGS__)
#define EE_LOG_ERROR(...) ::ee::log::write(::ee::log::Level::Error, __VA_ARGS__)
