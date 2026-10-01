#pragma once
#include "scripting/Syntax.h"
#include <filesystem>
namespace game::script {
struct CheckResult { size_t files = 0; std::vector<Diagnostic> diagnostics; };
// Parses the entry and all statically referenced imports, without executing code.
CheckResult checkScripts(const std::filesystem::path& root,const std::string& entry);
}
