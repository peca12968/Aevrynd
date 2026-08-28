#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace Aevrynd {
enum class MaterialDetailClass3D : uint8_t {Distant,Environment,Near,Hero};
enum class MaterialQuality3D : uint8_t {Low,Medium,High,Ultra};
struct MaterialResolutionPolicy3D {
    unsigned distantResolution{512},environmentResolution{1024},nearResolution{2048},heroResolution{4096};
    unsigned maximum{4096};
    constexpr unsigned Select(MaterialDetailClass3D detail,MaterialQuality3D quality=MaterialQuality3D::Ultra)const {
        unsigned resolution=detail==MaterialDetailClass3D::Distant?distantResolution:detail==MaterialDetailClass3D::Environment?environmentResolution:detail==MaterialDetailClass3D::Near?nearResolution:heroResolution;
        const unsigned qualityLimit=quality==MaterialQuality3D::Low?512:quality==MaterialQuality3D::Medium?1024:quality==MaterialQuality3D::High?2048:4096;
        return (std::min)({resolution,maximum,qualityLimit});
    }
};
constexpr std::size_t EstimateTextureBytes(unsigned width,unsigned height,unsigned channels=4,bool mipmaps=true){const std::size_t base=std::size_t(width)*height*channels;return mipmaps?base+base/3:base;}
constexpr std::size_t EstimatePbrMaterialBytes(unsigned resolution,unsigned textureCount=3,bool mipmaps=true){return EstimateTextureBytes(resolution,resolution,4,mipmaps)*textureCount;}
constexpr std::size_t EstimateBlockCompressedTextureBytes(unsigned width,unsigned height,unsigned blockBytes=8,bool mipmaps=true){std::size_t bytes{};do{bytes+=std::size_t((width+3)/4)*((height+3)/4)*blockBytes;if(!mipmaps||(width==1&&height==1))break;width=(std::max)(1u,width/2);height=(std::max)(1u,height/2);}while(true);return bytes;}
constexpr std::size_t EstimateCompressedPbrMaterialBytes(unsigned resolution,bool mipmaps=true){return EstimateBlockCompressedTextureBytes(resolution,resolution,8,mipmaps)+EstimateBlockCompressedTextureBytes(resolution,resolution,16,mipmaps)*2;}
}
