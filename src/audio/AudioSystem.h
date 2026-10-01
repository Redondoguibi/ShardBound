#pragma once
#include <filesystem>
#include <memory>
#include <vector>
namespace game {
class AudioSystem {
public:
    explicit AudioSystem(bool offline=false);
    ~AudioSystem();
    void play(const std::filesystem::path& file,bool streaming=false);
    void update();
    std::vector<float> mix(unsigned frames);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
