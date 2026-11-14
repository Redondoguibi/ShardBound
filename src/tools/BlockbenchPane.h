#pragma once
#include <windows.h>
#include <filesystem>
#include <memory>
#include <functional>
#include <string>
#include <nlohmann/json.hpp>

namespace game::editor {
class BlockbenchPane {
public:
    using BlockEvent=std::function<void(const nlohmann::json&)>;
    BlockbenchPane(HWND owner,std::filesystem::path profile,std::wstring script={},BlockEvent event={});
    ~BlockbenchPane();
    void show(const RECT& bounds,bool visible);
    void retry();
    void execute(const std::wstring& code);
    bool owns(HWND window) const;
    std::wstring status() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
}
