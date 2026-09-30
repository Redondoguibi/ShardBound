#include "core/GameLoop.h"
#include <iostream>
#include <stdexcept>
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        game::GameLoop loop;
        unsigned frames = 0;
        loop.run({0, 3}, [&](double dt) { require(dt >= 0, "negative delta"); ++frames; });
        require(frames == 3, "frame limit was ignored");
        frames = 0;
        loop.run({0, 0}, [&](double) { ++frames; loop.stop(); });
        require(frames == 1, "stop was ignored");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
