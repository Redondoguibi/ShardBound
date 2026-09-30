#pragma once
#include <string_view>
namespace game {
enum class LogLevel { Info, Warning, Error };
void log(LogLevel level, std::string_view subsystem, std::string_view message);
}
#define LOG_INFO(system, message) ::game::log(::game::LogLevel::Info, system, message)
#define LOG_WARN(system, message) ::game::log(::game::LogLevel::Warning, system, message)
#define LOG_ERROR(system, message) ::game::log(::game::LogLevel::Error, system, message)
