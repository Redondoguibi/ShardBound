#pragma once
#include <filesystem>
#include <memory>
#include <vector>
#include <cstdint>
namespace game {
enum class AudioBus { Effects,Music,Interface,Count };
void validateAudio(const std::filesystem::path& file);
class AudioSystem {
public:
    explicit AudioSystem(bool offline=false);
    ~AudioSystem();
    void play(const std::filesystem::path& file,bool streaming=false,AudioBus bus=AudioBus::Effects);
    std::uint64_t playVoice(const std::filesystem::path& file,float volume=1,float pitch=1,bool loop=false,double offset=0,bool streaming=false,AudioBus bus=AudioBus::Effects);
    void stopVoice(std::uint64_t voice);
    void pauseVoice(std::uint64_t voice,bool paused);
    bool voiceActive(std::uint64_t voice) const;
    size_t activeVoices() const;
    void update();
    void setVolume(float volume);
    void setBusVolume(AudioBus bus,float volume);
    std::vector<float> mix(unsigned frames);
    bool offline() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
