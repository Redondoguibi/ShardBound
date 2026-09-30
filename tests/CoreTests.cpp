#include "core/GameLoop.h"
#include "platform/InputSystem.h"
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
        for (const auto rate : {30, 60, 120, 144}) {
            game::FixedStepper stepper;
            unsigned steps = 0;
            for (int i = 0; i < rate; ++i) steps += stepper.advance(1.0 / rate);
            require(steps == 60, "simulation depends on rendering frequency");
        }
        game::FixedStepper stepper;
        require(stepper.advance(10) == 15, "unbounded catch-up after stall");
        game::InputSystem input;
        game::Event key; key.type = game::EventType::KeyDown; key.key = game::Key::W;
        input.process(key);
        require(input.pressed("MoveForward") && input.moveY() == 1, "key binding failed");
        input.beginFrame(); input.process(key);
        require(!input.pressed("MoveForward") && input.held("MoveForward"), "repeated key generated edge");
        input.process({game::EventType::FocusLost});
        require(!input.held("MoveForward") && input.released("MoveForward"), "focus loss leaves stuck key");
        input.process(key);
        require(!input.held("MoveForward"), "unfocused input accepted");
        input.process({game::EventType::FocusGained});
        input.bind("custom:action", {{game::Key::W}}); input.process(key);
        require(input.held("custom:action"), "custom action failed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
