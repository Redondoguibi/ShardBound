#include "audio/AudioSystem.h"
#include "core/Diagnostic.h"
#include <cmath>
#include <array>
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#undef STB_VORBIS_HEADER_ONLY
#pragma warning(push)
// stb's second interpolation probe reads mid only after a successful first probe.
// MSVC's optimizer loses that relationship after inlining its seek implementation.
#pragma warning(disable:4701)
#include <stb_vorbis.c>
#pragma warning(pop)
#undef L
#undef C
#undef R
namespace game {
void validateAudio(const std::filesystem::path& file) {
    ma_decoder decoder{};if(ma_decoder_init_file_w(file.c_str(),nullptr,&decoder)!=MA_SUCCESS)throw DiagnosticError({"AUDIO110","Unsupported or corrupt audio",file.string()});
    ma_uint64 length=0;const auto result=ma_decoder_get_length_in_pcm_frames(&decoder,&length);ma_decoder_uninit(&decoder);if(result!=MA_SUCCESS||length==0)throw DiagnosticError({"AUDIO110","Audio has no decodable frames",file.string()});
}
struct AudioSystem::Impl {
    ma_engine engine{}; bool initialized=false,offline=false;
    std::uint64_t next=1;
    std::array<ma_sound_group,size_t(AudioBus::Count)> groups{};
    size_t initializedGroups=0;
    struct Sound { ma_sound value{}; std::uint64_t id=0; bool initialized=false; ~Sound() { if(initialized) ma_sound_uninit(&value); } };
    std::vector<std::unique_ptr<Sound>> sounds;
    ~Impl() { sounds.clear();while(initializedGroups) ma_sound_group_uninit(&groups[--initializedGroups]);if(initialized) ma_engine_uninit(&engine); }
};
AudioSystem::AudioSystem(bool offline) : impl_(std::make_unique<Impl>()) {
    auto config=ma_engine_config_init(); config.noDevice=offline; config.channels=2; config.sampleRate=48000;
    if(ma_engine_init(&config,&impl_->engine)!=MA_SUCCESS) throw DiagnosticError({"AUDIO101","Audio engine initialization failed"});
    impl_->initialized=true; impl_->offline=offline;
    for(auto& group:impl_->groups) {if(ma_sound_group_init(&impl_->engine,0,nullptr,&group)!=MA_SUCCESS) throw DiagnosticError({"AUDIO111","Cannot create audio bus"});++impl_->initializedGroups;}
}
AudioSystem::~AudioSystem()=default;
bool AudioSystem::offline() const { return impl_->offline; }
void AudioSystem::play(const std::filesystem::path& file,bool streaming,AudioBus bus) {
    (void)playVoice(file,1,1,false,0,streaming,bus);
}
std::uint64_t AudioSystem::playVoice(const std::filesystem::path& file,float volume,float pitch,bool loop,double offset,bool streaming,AudioBus bus) {
    if(size_t(bus)>=impl_->groups.size()) throw DiagnosticError({"AUDIO111","Invalid audio bus"});
    if(!std::isfinite(volume)||volume<0||volume>1||!std::isfinite(pitch)||pitch<.01f||pitch>16||!std::isfinite(offset)||offset<0) throw DiagnosticError({"AUDIO107","Invalid voice parameters"});
    if(impl_->sounds.size()>=256) throw DiagnosticError({"AUDIO108","Voice limit exceeded (256)"});
    auto sound=std::make_unique<Impl::Sound>();
    if(ma_sound_init_from_file_w(&impl_->engine,file.c_str(),(streaming ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE)|MA_SOUND_FLAG_NO_SPATIALIZATION,&impl_->groups[size_t(bus)],nullptr,&sound->value)!=MA_SUCCESS)
        throw DiagnosticError({"AUDIO102","Cannot load sound",file.string()});
    sound->initialized=true;
    sound->id=impl_->next++; const auto id=sound->id;
    ma_sound_set_volume(&sound->value,volume); ma_sound_set_pitch(&sound->value,pitch); ma_sound_set_looping(&sound->value,loop);
    if(offset>0) { ma_uint32 rate=0; ma_uint64 length=0; ma_sound_get_data_format(&sound->value,nullptr,nullptr,&rate,nullptr,0); ma_sound_get_length_in_pcm_frames(&sound->value,&length);
        auto frame=static_cast<ma_uint64>(offset*rate); if(loop&&length) frame%=length; if(length&&frame>=length) frame=length-1;
        if(ma_sound_seek_to_pcm_frame(&sound->value,frame)!=MA_SUCCESS) throw DiagnosticError({"AUDIO109","Cannot seek sound",file.string()}); }
    if(ma_sound_start(&sound->value)!=MA_SUCCESS) throw DiagnosticError({"AUDIO103","Cannot start sound",file.string()});
    impl_->sounds.push_back(std::move(sound));
    return id;
}
void AudioSystem::stopVoice(std::uint64_t voice) { std::erase_if(impl_->sounds,[&](const auto& s){return s->id==voice;}); }
void AudioSystem::pauseVoice(std::uint64_t voice,bool paused) { for(const auto& s:impl_->sounds) if(s->id==voice) { if(paused) ma_sound_stop(&s->value); else ma_sound_start(&s->value); } }
bool AudioSystem::voiceActive(std::uint64_t voice) const { for(const auto& s:impl_->sounds) if(s->id==voice) return !ma_sound_at_end(&s->value); return false; }
size_t AudioSystem::activeVoices() const { return impl_->sounds.size(); }
void AudioSystem::setVolume(float volume) {
    if(!std::isfinite(volume) || volume<0 || volume>1) throw DiagnosticError({"AUDIO106","Volume must be in [0,1]"});
    ma_engine_set_volume(&impl_->engine,volume);
}
void AudioSystem::setBusVolume(AudioBus bus,float volume) {
    if(size_t(bus)>=impl_->groups.size() || !std::isfinite(volume) || volume<0 || volume>1) throw DiagnosticError({"AUDIO111","Invalid audio bus volume"});
    ma_sound_group_set_volume(&impl_->groups[size_t(bus)],volume);
}
void AudioSystem::update() { std::erase_if(impl_->sounds,[](const auto& sound) { return ma_sound_at_end(&sound->value)!=0; }); }
std::vector<float> AudioSystem::mix(unsigned frames) {
    if(!impl_->offline || frames>480000) throw DiagnosticError({"AUDIO104","Manual mixing requires offline engine and at most 10 seconds"});
    std::vector<float> result(size_t(frames)*2);
    if(ma_engine_read_pcm_frames(&impl_->engine,result.data(),frames,nullptr)!=MA_SUCCESS) throw DiagnosticError({"AUDIO105","Audio mix failed"});
    return result;
}
}
