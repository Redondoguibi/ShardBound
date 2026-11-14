#pragma once
#include <nlohmann/json_fwd.hpp>
#include <filesystem>
#include <functional>
#include <stop_token>

namespace game {
enum class AiProvider { Codex, OpenRouter, OpenAI, Anthropic, Google };
struct EditorAiRequest {
    AiProvider provider=AiProvider::Codex;
    std::string model,prompt;
    std::filesystem::path work,programDirectory;
    bool autonomous=false;
};
using AiReview=std::function<bool(const std::wstring&)>;
using AiStatus=std::function<void(const std::string&)>;
bool editorAiHasKey(AiProvider provider);
void editorAiSaveKey(AiProvider provider,std::wstring key);
using AiTools=std::function<nlohmann::json(const nlohmann::json&)>;
using AiResponse=std::function<std::string(const std::string&)>;
std::string runEditorAi(const EditorAiRequest& request,const AiTools& tools,const AiReview& review,const AiStatus& status,
    std::stop_token stop,const AiResponse& responseOverride={});
}
