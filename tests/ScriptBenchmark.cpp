#include "scripting/Runtime.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace game::script;
int main() {
    try {
        Runtime runtime;
        runtime.execute("bench",R"SL(
func scale(x, factor = 1.25):
    return x * factor + 0.75
func calculate():
    total = 0
    for index in number_range(128):
        a = scale(index)
        b = scale(a, 0.5)
        total = total + a + b
    return total
)SL");
        const auto function=runtime.global("bench","calculate");
        for(int i=0;i<50;++i) runtime.invoke(function,{});
        std::vector<double> samples;
        for(int sample=0;sample<7;++sample) {
            const auto start=std::chrono::steady_clock::now();
            for(int i=0;i<500;++i) if(runtime.invoke(function,{}).number()!=15480) throw std::runtime_error("Benchmark computation changed");
            samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        }
        std::sort(samples.begin(),samples.end());
        std::cout<<"{\"invocations_per_sample\":500,\"iterations_per_invocation\":128,\"samples\":7,\"median_ms\":"<<samples[3]<<",\"min_ms\":"<<samples.front()<<",\"max_ms\":"<<samples.back()<<"}\n";return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
