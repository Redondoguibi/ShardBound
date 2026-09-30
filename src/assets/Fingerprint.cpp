#include "assets/Fingerprint.h"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <fstream>
#include <stdexcept>
namespace game {
std::string fingerprint(const std::filesystem::path& source, std::string_view settings) {
    std::ifstream stream(source,std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read asset: " + source.string());
    BCRYPT_HASH_HANDLE handle = nullptr;
    if (BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE,&handle,nullptr,0,nullptr,0,0) < 0) throw std::runtime_error("SHA256 initialization failed");
    struct Guard { BCRYPT_HASH_HANDLE handle; ~Guard() { BCryptDestroyHash(handle); } } guard{handle};
    auto hash = [&](const void* data, size_t count) {
        if (count > ULONG_MAX || BCryptHashData(handle,static_cast<PUCHAR>(const_cast<void*>(data)),static_cast<ULONG>(count),0) < 0)
            throw std::runtime_error("SHA256 update failed");
    };
    hash(settings.data(),settings.size());
    std::array<char,65536> chunk;
    while (stream.read(chunk.data(),chunk.size()) || stream.gcount()) hash(chunk.data(),static_cast<size_t>(stream.gcount()));
    if (!stream.eof()) throw std::runtime_error("Asset read failed: " + source.string());
    std::array<unsigned char,32> digest{};
    if (BCryptFinishHash(handle,digest.data(),static_cast<ULONG>(digest.size()),0) < 0) throw std::runtime_error("SHA256 finalization failed");
    std::string value; constexpr char hex[] = "0123456789abcdef";
    for (auto byte : digest) { value += hex[byte >> 4]; value += hex[byte & 15]; }
    return value;
}
}
