#include "assets/AssetId.h"
#include "core/Diagnostic.h"
#include <algorithm>
namespace game {
bool validNamespace(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(),value.end(),[](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
}
AssetId::AssetId(std::string value) : value_(std::move(value)) {
    const auto colon = value_.find(':');
    const auto path = colon == std::string::npos ? std::string_view{} : std::string_view(value_).substr(colon+1);
    bool valid = colon != std::string::npos && validNamespace(std::string_view(value_).substr(0,colon)) && !path.empty();
    size_t start = 0;
    while (start <= path.size()) {
        const auto end = path.find('/',start);
        const auto part = path.substr(start,end == std::string_view::npos ? path.size()-start : end-start);
        valid &= !part.empty() && part != "." && part != ".." && std::all_of(part.begin(),part.end(),[](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        });
        if (end == std::string_view::npos) break;
        start = end+1;
    }
    if (!valid) throw DiagnosticError({"ASSET101","Invalid namespace:path ID",{}, {},value_});
}
std::string AssetId::nameSpace() const { return value_.substr(0,value_.find(':')); }
}
