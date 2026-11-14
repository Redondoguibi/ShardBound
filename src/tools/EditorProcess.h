#pragma once
#include <filesystem>
#include <stop_token>
#include <string>
#include <vector>

namespace game {
struct EditorProcessResult { unsigned exitCode;std::string output; };
EditorProcessResult runEditorProcess(const std::wstring& program,const std::vector<std::wstring>& args,
    const std::filesystem::path& directory,std::string input,std::stop_token stop);
}
