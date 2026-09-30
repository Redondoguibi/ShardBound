#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <stdexcept>
#include <format>
namespace game::d3d {
template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
inline void check(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(std::format("D3D11 {} failed: HRESULT 0x{:08X}", operation, static_cast<unsigned long>(result)));
}
}
