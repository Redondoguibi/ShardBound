#include "core/FrameProfiler.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace game {
using Json=nlohmann::json;
namespace {
Json metric(std::vector<double> values) {
    if(values.empty()) return nullptr;
    std::sort(values.begin(),values.end());double sum=0;for(auto v:values) sum+=v;
    auto percentile=[&](double q){return values[size_t(std::ceil(q*values.size()))-1];};
    return {{"samples",values.size()},{"mean",sum/values.size()},{"p50",percentile(.5)},{"p95",percentile(.95)},{"p99",percentile(.99)},{"max",values.back()}};
}
}
void FrameProfiler::enabled(bool value) {if(enabled_==value) return;enabled_=value;if(value) {samples_.clear();total_=0;}}
void FrameProfiler::sample(FrameSample value) {
    if(!enabled_) return;
    for(auto v:{value.frame,value.cpu,value.present,value.simulation,value.streaming}) if(!std::isfinite(v)||v<0) throw std::invalid_argument("Invalid frame metric");
    if(!std::isfinite(value.gpu)||value.gpu< -1) throw std::invalid_argument("Invalid GPU metric");
    if(samples_.size()==600) samples_.pop_front();samples_.push_back(value);++total_;
}
Data FrameProfiler::report(const std::string& device,const Data& metadata) const {
    std::vector<double> frame,cpu,gpu,present,simulation,streaming;Json rows=Json::array();
    for(const auto& s:samples_) {frame.push_back(s.frame);cpu.push_back(s.cpu);if(s.gpu>=0) gpu.push_back(s.gpu);present.push_back(s.present);simulation.push_back(s.simulation);streaming.push_back(s.streaming);
        rows.push_back({{"frame_ms",s.frame},{"cpu_ms",s.cpu},{"gpu_ms",s.gpu<0?Json(nullptr):Json(s.gpu)},{"present_ms",s.present},{"simulation_ms",s.simulation},{"streaming_ms",s.streaming},{"private_bytes",s.memory},{"draws",s.draws},{"entities",s.entities},{"chunks",s.chunks},{"gameplay",s.gameplay}});}
    Json j={{"format","shardbound-performance"},{"version",1},{"platform","Windows x64 / D3D11"},{"gpu",device},{"enabled",enabled_},{"total_frames",total_},{"window_frames",samples_.size()},{"window_capacity",600},{"gpu_timing","asynchronous timestamps; samples may arrive several frames later"},{"units","milliseconds / bytes"},{"metadata",Json::parse(encodeData(metadata))},{"metrics",{{"frame",metric(frame)},{"cpu",metric(cpu)},{"gpu",metric(gpu)},{"present",metric(present)},{"simulation",metric(simulation)},{"streaming",metric(streaming)}}},{"frames",rows}};
#ifdef NDEBUG
    j["build"]="Release";
#else
    j["build"]="Debug";
#endif
    return decodeData(j.dump());
}
std::string FrameProfiler::summary() const {
    if(samples_.empty()) return "Desempenho: aguardando quadros...";
    std::vector<double> frame,cpu,gpu;for(const auto& s:samples_) {frame.push_back(s.frame);cpu.push_back(s.cpu);if(s.gpu>=0) gpu.push_back(s.gpu);}
    const auto f=metric(frame),c=metric(cpu),g=metric(gpu);std::ostringstream out;out<<std::fixed<<std::setprecision(2)<<"Desempenho | "<<(f["mean"].get<double>()>0?1000/f["mean"].get<double>():0)<<" FPS | janela: "<<samples_.size()<<" quadros\nQuadro P50/P95/P99: "<<f["p50"].get<double>()<<" / "<<f["p95"].get<double>()<<" / "<<f["p99"].get<double>()<<" ms\nCPU media: "<<c["mean"].get<double>()<<" ms | GPU: ";
    if(g.is_null()) out<<"aguardando amostra valida";else out<<g["mean"].get<double>()<<" ms (consulta atrasada)";
    out<<"\nMemoria privada: "<<samples_.back().memory/(1024*1024)<<" MiB | draws: "<<samples_.back().draws;
    return out.str();
}
}
