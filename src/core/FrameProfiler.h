#pragma once
#include "core/Data.h"
#include <deque>
#include <cstdint>
namespace game {
struct FrameSample {double frame=0,cpu=0,gpu=-1,present=0,simulation=0,streaming=0;std::uint64_t memory=0,draws=0,entities=0,chunks=0;bool gameplay=false;};
class FrameProfiler {
public:
    void enabled(bool value);
    bool enabled() const { return enabled_; }
    void sample(FrameSample sample);
    Data report(const std::string& device,const Data& metadata={}) const;
    std::string summary() const;
    std::vector<double> history() const {std::vector<double> result;for(size_t i=samples_.size()>120?samples_.size()-120:0;i<samples_.size();++i) result.push_back(samples_[i].frame);return result;}
    size_t size() const { return samples_.size(); }
private:
    bool enabled_=false;
    std::uint64_t total_=0;
    std::deque<FrameSample> samples_;
};
}
