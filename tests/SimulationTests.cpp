#include "physics/PhysicsWorld.h"
#include "audio/AudioSystem.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
int main() {
    try {
        game::PhysicsWorld physics;
        physics.box({0,-0.5f,0},{10,0.5f,10},false);
        auto body=physics.box({0,3,0},{0.5f,0.5f,0.5f},true);
        for(int i=0;i<180;++i) physics.step(1.0f/60);
        const auto position=physics.position(body);
        if(position.y<0.45f || position.y>0.6f) throw std::runtime_error("Jolt body did not settle on floor");
        physics.destroy(body); bool rejected=false;
        try { physics.position(body); } catch(...) { rejected=true; }
        if(!rejected) throw std::runtime_error("Destroyed physics body accepted");
        game::AudioSystem audio(true); audio.play("assets/core/audio/hit.wav");
        auto samples=audio.mix(4800);
        if(std::none_of(samples.begin(),samples.end(),[](float x) { return std::abs(x)>0.001f; })) throw std::runtime_error("Decoded sound is silent");
        audio.update(); return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
