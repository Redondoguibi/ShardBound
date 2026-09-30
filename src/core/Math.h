#pragma once
#include <array>
namespace game {
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Vec2 { float x = 0, y = 0; };
struct Color { float r = 1, g = 1, b = 1, a = 1; };
struct Transform { Vec3 position{}, rotation{}, scale{1, 1, 1}; };
struct Matrix { std::array<float, 16> values{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; };
Matrix transformMatrix(const Transform& transform);
Matrix multiply(const Matrix& local, const Matrix& parent);
}
