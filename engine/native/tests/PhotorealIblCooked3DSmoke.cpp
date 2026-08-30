#include <Aevrynd/EnvironmentIbl3D.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
bool Patch32(const std::filesystem::path& path, std::size_t offset, std::uint32_t value) {
    std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) return false;
    std::array<char, 4> bytes{};
    for (unsigned index = 0; index < 4; ++index) bytes[index] = static_cast<char>(value >> (index * 8));
    file.seekp(static_cast<std::streamoff>(offset)); file.write(bytes.data(), bytes.size()); return bool(file);
}
}

int main(int argc, char** argv) {
    using namespace Aevrynd;
    if (argc != 2) return 100;
    const std::filesystem::path root = std::filesystem::absolute(argv[1]);
    EnvironmentIblValidatedData3D data;
    const EnvironmentIblDefinition3D definition{root / "environment-specular.dds", root / "environment-irradiance.dds", root / "environment-brdf.dds", 7.0f};
    if (!LoadAndValidateEnvironmentIbl3D(definition, data)) return 1;
    if (data.specular.width != 128 || data.specular.faceCount != 6 || data.specular.mipCount != 8 ||
        data.irradiance.width != 32 || data.irradiance.faceCount != 6 || data.irradiance.mipCount != 6 ||
        data.brdf.width != 128 || data.brdf.height != 128 || data.brdf.faceCount != 1 || data.brdf.mipCount != 1 ||
        data.payloadBytes == 0 || !std::isfinite(data.rotationRadians) || data.rotationRadians < 0.f || data.rotationRadians >= 6.2831854f) return 2;
    EnvironmentLightingStore3D store;
    const auto handle = store.Create(definition);
    if (!handle || !store.Resolve(handle) || store.MemoryStats().residentInstances != 1 || !store.MemoryStats().payloadBytes) return 4;
    store.Destroy(handle);
    store.Trim();
    const auto trimmed = store.MemoryStats();
    if (trimmed.residentInstances || trimmed.residentReferences || trimmed.payloadSets || trimmed.payloadBytes || trimmed.sourceSets || trimmed.instanceCacheEntries || trimmed.payloadCacheEntries || trimmed.rawSourceEntries || trimmed.canonicalSourceEntries || trimmed.reclaimableBytes) return 5;
    store.Shutdown();
    EnvironmentIblValidatedData3D sentinel = data;
    auto invalid = definition; invalid.rotationRadians = std::numeric_limits<float>::quiet_NaN();
    if (LoadAndValidateEnvironmentIbl3D(invalid, sentinel) || sentinel.payloadBytes != data.payloadBytes) return 3;
    const auto hostile = std::filesystem::temp_directory_path() / "aevrynd-photoreal-ibl-hostile";
    std::error_code fileError; std::filesystem::remove_all(hostile, fileError); std::filesystem::create_directories(hostile, fileError);
    if (fileError) return 6;
    for (const auto* name : {"environment-specular.dds", "environment-irradiance.dds", "environment-brdf.dds"})
        if (!std::filesystem::copy_file(root / name, hostile / name, std::filesystem::copy_options::overwrite_existing, fileError) || fileError) return 7;
    const EnvironmentIblDefinition3D hostileDefinition{hostile / "environment-specular.dds", hostile / "environment-irradiance.dds", hostile / "environment-brdf.dds", 0.f};
    auto rejected = [&](const char* name, std::size_t offset, std::uint32_t value) {
        std::filesystem::copy_file(root / name, hostile / name, std::filesystem::copy_options::overwrite_existing, fileError);
        if (fileError || !Patch32(hostile / name, offset, value)) return false;
        EnvironmentIblValidatedData3D unchanged = data;
        return !LoadAndValidateEnvironmentIbl3D(hostileDefinition, unchanged) && unchanged.payloadBytes == data.payloadBytes;
    };
    if (!rejected("environment-specular.dds", 128, 98) || !rejected("environment-specular.dds", 28, 7) ||
        !rejected("environment-irradiance.dds", 28, 5) || !rejected("environment-brdf.dds", 128, 95)) return 8;
    std::filesystem::copy_file(root / "environment-specular.dds", hostile / "environment-specular.dds", std::filesystem::copy_options::overwrite_existing, fileError);
    std::filesystem::resize_file(hostile / "environment-specular.dds", std::filesystem::file_size(hostile / "environment-specular.dds") - 1, fileError);
    EnvironmentIblValidatedData3D unchanged = data;
    if (fileError || LoadAndValidateEnvironmentIbl3D(hostileDefinition, unchanged) || unchanged.payloadBytes != data.payloadBytes) return 9;
    std::filesystem::remove_all(hostile, fileError);
    std::cout << "specularFaces=6 specularMips=8 irradianceMips=6 brdf=RG16_FLOAT payload=" << data.payloadBytes << '\n';
    return 0;
}
