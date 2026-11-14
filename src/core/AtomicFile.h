#pragma once
#include <filesystem>
#include <functional>
#include <string_view>
namespace game {
enum class WriteStage { Flushed,Published };
void flushFile(const std::filesystem::path& path);
// Rename without replacing a destination; tolerate short-lived Windows sharing locks.
void renameDirectory(const std::filesystem::path& source,const std::filesystem::path& destination);
void publishFile(const std::filesystem::path& temporary,const std::filesystem::path& destination,bool backup=false);
void atomicWrite(const std::filesystem::path& destination,std::string_view data,bool backup=false,const std::function<void(WriteStage)>& observer={});
}
