#include "core/Log.h"
#include <iostream>
#include <syncstream>
namespace game {
void log(LogLevel level, std::string_view subsystem, std::string_view message) {
    const char* label = level == LogLevel::Error ? "ERROR" : level == LogLevel::Warning ? "WARN" : "INFO";
    std::osyncstream(level == LogLevel::Error ? std::cerr : std::cout)
        << '[' << subsystem << "][" << label << "] " << message << '\n';
}
}
