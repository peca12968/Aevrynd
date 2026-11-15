#pragma once
#include "MistwatchSettingsFile.h"
#include <array>
#include <fstream>

namespace Mistwatch {
struct GraphicsSettings {
    bool sunShadows{true};
    bool ambientShading{true};

    bool Toggle(bool sun,bool ambient) noexcept {
        if(sun)sunShadows=!sunShadows;
        if(ambient)ambientShading=!ambientShading;
        return sun||ambient;
    }

    constexpr std::string_view Serialize() const noexcept {
        constexpr std::array<std::string_view,4> choices{
            "MISTWATCH_GRAPHICS 1 0 0","MISTWATCH_GRAPHICS 1 0 1",
            "MISTWATCH_GRAPHICS 1 1 0","MISTWATCH_GRAPHICS 1 1 1"};
        return choices[2u*unsigned(sunShadows)+unsigned(ambientShading)];
    }

    bool Restore(std::string_view text) noexcept {
        constexpr std::string_view prefix="MISTWATCH_GRAPHICS 1 ";
        if(text.size()!=prefix.size()+3||!text.starts_with(prefix))return false;
        const auto flags=text.substr(prefix.size());
        if((flags[0]!='0'&&flags[0]!='1')||flags[1]!=' '||(flags[2]!='0'&&flags[2]!='1'))return false;
        sunShadows=flags[0]=='1';
        ambientShading=flags[2]=='1';
        return true;
    }
};

inline bool ReadGraphicsSettingsFile(const std::filesystem::path& file,GraphicsSettings& settings) {
    std::ifstream in(ProfileIoPath(file),std::ios::binary);
    std::array<char,GraphicsSettings{}.Serialize().size()+1> text{};
    in.read(text.data(),std::streamsize(text.size()));
    const auto bytes=in.gcount();
    return !in.bad()&&bytes>0&&settings.Restore(std::string_view(text.data(),std::size_t(bytes)));
}
}
