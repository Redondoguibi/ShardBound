#pragma once
#include "extensions/Components.h"
#include "persistence/SaveGame.h"
#include "scripting/Bridge.h"
#include <optional>
namespace game {
class ExtensionBridge {
public:
    ExtensionBridge(script::Runtime& runtime,World& world,Components& components,SaveGame& saves);
    // Safe point outside any script callback. At most one pending operation.
    std::vector<Diagnostic> process();
private:
    script::Runtime& runtime_; SaveGame& saves_;
    std::optional<std::pair<bool,std::string>> pending_;
};
}
