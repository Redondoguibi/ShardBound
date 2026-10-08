#include "audio/AudioSystem.h"
#include "core/Diagnostic.h"
#include <cmath>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
namespace game {
struct AudioSystem::Impl {
    ma_engine engine{}; bool initialized=false,offline=false;
    struct Sound { ma_sound value{}; bool initialized=false; ~Sound() { if(initialized) ma_sound_uninit(&value); } };
    std::vector<std::unique_ptr<Sound>> sounds;
    ~Impl() { sounds.clear(); if(initialized) ma_engine_uninit(&engine); }
};
AudioSystem::AudioSystem(bool offline) : impl_(std::make_unique<Impl>()) {
    auto config=ma_engine_config_init(); config.noDevice=offline; config.channels=2; config.sampleRate=48000;
    if(ma_engine_init(&config,&impl_->engine)!=MA_SUCCESS) throw DiagnosticError({"AUDIO101","Audio engine initialization failed"});
    impl_->initialized=true; impl_->offline=offline;
}
AudioSystem::~AudioSystem()=default;
bool AudioSystem::offline() const { return impl_->offline; }
void AudioSystem::play(const std::filesystem::path& file,bool streaming) {
    auto sound=std::make_unique<Impl::Sound>();
    if(ma_sound_init_from_file_w(&impl_->engine,file.c_str(),streaming ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE,nullptr,nullptr,&sound->value)!=MA_SUCCESS)
        throw DiagnosticError({"AUDIO102","Cannot load sound",file.string()});
    sound->initialized=true;
    if(ma_sound_start(&sound->value)!=MA_SUCCESS) throw DiagnosticError({"AUDIO103","Cannot start sound",file.string()});
    impl_->sounds.push_back(std::move(sound));
}
void AudioSystem::setVolume(float volume) {
    if(!std::isfinite(volume) || volume<0 || volume>1) throw DiagnosticError({"AUDIO106","Volume must be in [0,1]"});
    ma_engine_set_volume(&impl_->engine,volume);
}
void AudioSystem::update() { std::erase_if(impl_->sounds,[](const auto& sound) { return ma_sound_at_end(&sound->value)!=0; }); }
std::vector<float> AudioSystem::mix(unsigned frames) {
    if(!impl_->offline || frames>480000) throw DiagnosticError({"AUDIO104","Manual mixing requires offline engine and at most 10 seconds"});
    std::vector<float> result(size_t(frames)*2);
    if(ma_engine_read_pcm_frames(&impl_->engine,result.data(),frames,nullptr)!=MA_SUCCESS) throw DiagnosticError({"AUDIO105","Audio mix failed"});
    return result;
}
}
