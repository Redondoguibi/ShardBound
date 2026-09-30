#pragma once
#include <string>
#include <string_view>
namespace game {
bool validNamespace(std::string_view value);
class AssetId {
public:
    explicit AssetId(std::string value);
    const std::string& str() const { return value_; }
    std::string nameSpace() const;
private:
    std::string value_;
};
}
