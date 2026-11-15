#pragma once
#include "MistwatchCameraSettings.h"
#include "MistwatchSettingsFile.h"
#include <array>
#include <fstream>

namespace Mistwatch {
inline bool ReadCameraSettingsFile(const std::filesystem::path& file,CameraSettings& settings) {
    std::ifstream in(ProfileIoPath(file),std::ios::binary);std::array<char,129> text{};
    in.read(text.data(),std::streamsize(text.size()));
    const auto bytes=in.gcount();
    if(in.bad()||bytes<=0||bytes>128)return false;
    return settings.Restore(std::string_view(text.data(),std::size_t(bytes)));
}
inline bool WriteCameraSettingsFile(const std::filesystem::path& file,const CameraSettings& settings) {
    return WriteSettingsFile(file,settings.Serialize());
}
}
