#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace game {
struct TextureData { unsigned width = 0, height = 0; std::vector<std::uint8_t> pixels; bool cacheHit = false; };
TextureData compileTexture(const std::filesystem::path& source, const std::filesystem::path& cache, bool srgb, bool nearest);
TextureData decodeTexture(const std::uint8_t* bytes, size_t size);
}
