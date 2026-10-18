#include "MistwatchNettleAsset.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/NativeMaterialDds3D.h>
#include <bit>
#include <filesystem>
#include <iostream>
#include <limits>

namespace {
using namespace Aevrynd;
using namespace Mistwatch;
void Require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
bool Same(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
std::uint64_t MeshHash(const Mesh3D& mesh) {
    std::uint64_t hash=1469598103934665603ull;
    const auto mix=[&](std::uint32_t value){for(unsigned byte=0;byte<4;++byte){hash^=(value>>(byte*8))&255u;hash*=1099511628211ull;}};
    mix(static_cast<std::uint32_t>(mesh.vertices.size()));mix(static_cast<std::uint32_t>(mesh.indices.size()));
    for(const auto& vertex:mesh.vertices) {
        for(float value:{vertex.x,vertex.y,vertex.z,vertex.u,vertex.v})mix(std::bit_cast<std::uint32_t>(value));
        mix(vertex.normal);mix(vertex.color);
    }
    for(const auto index:mesh.indices)mix(index);return hash;
}
}
int main() {
    try {
        const auto root=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets/mistwatch-nettle-plant";
        Require(NettleVariantCount==6&&NettleAssetScale==4.f&&NettleAssetProfile=="official-cc0-nettle-4x-v1","Unexpected nettle author identity");
        const auto definition=NettleMaterialDefinition();
        Require(definition.material.alphaMode==MaterialAlphaMode3D::Mask&&definition.material.alphaCutoff==.5f&&
            definition.material.doubleSided&&definition.material.materialClass==MaterialClass3D::Foliage&&
            definition.material.useUv&&definition.material.textureScale==1.f&&definition.material.castsShadow&&
            definition.material.tint.r==1.f&&definition.material.tint.g==1.f&&definition.material.tint.b==1.f&&definition.material.tint.a==1.f&&
            definition.textures.surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrm&&definition.textures.maximumResolution==2048&&
            definition.textures.alphaMode==MaterialAlphaMode3D::Mask,"Nettle material facade differs from complete scan");
        constexpr const char* files[]{"nettle-base.dds","nettle-arm.dds","nettle-normal.dds"};
        constexpr const char* seals[]{"1e875aa8967262dfef1cb8b3b372527eee308071948865489d263630fb826fb1",
            "895365555a197cf5f86feb088858cb0700cfc27f0ba05477bbd9407783cbacd0",
            "85729c0b6920f4d2c640c7fc038506c9071450de103a44ca2b6f2bde5451790f"};
        constexpr NativeMaterialDdsRole3D roles[]{NativeMaterialDdsRole3D::BaseColor,NativeMaterialDdsRole3D::Surface,NativeMaterialDdsRole3D::Normal};
        constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_BC7_UNORM_SRGB,DXGI_FORMAT_BC7_UNORM,DXGI_FORMAT_BC5_UNORM};
        unsigned negativeCases{};
        for(unsigned channel=0;channel<3;++channel) {
            const auto& integrity=definition.textures.integrity[channel];Require(integrity.expectedSha256==seals[channel]&&integrity.expectedBytes==5592580,"DDS seal channel mapping differs from cook");
            NativeMaterialDds3D image{};NativeMaterialDdsError3D error{};const auto path=root/"materials"/files[channel];
            Require(LoadNativeMaterialDds3D(path,roles[channel],2048,false,image,&error,seals[channel],5592580,true)&&
                image.width==2048&&image.height==2048&&image.mipCount==12&&image.format==formats[channel]&&image.sha256==seals[channel],
                "Real 2K complete-mip nettle DDS load failed");
            Require(!LoadNativeMaterialDds3D(path,roles[channel],2048,false,image,&error,std::string(64,'0'),5592580,true)&&
                error==NativeMaterialDdsError3D::HashMismatch,"Wrong DDS SHA accepted");++negativeCases;
            Require(!LoadNativeMaterialDds3D(path,roles[channel],2048,false,image,&error,seals[channel],5592579,true)&&
                error==NativeMaterialDdsError3D::HashMismatch,"Wrong DDS byte count accepted");++negativeCases;
            Require(!LoadNativeMaterialDds3D(path,roles[channel],1024,false,image,&error,seals[channel],5592580,true),"Wrong DDS resolution accepted");++negativeCases;
        }
        NativeMaterialDds3D invalidImage;
        Require(!LoadNativeMaterialDds3D(root/"materials"/files[1],roles[0],2048,false,invalidImage)&&
            !LoadNativeMaterialDds3D(root/"materials"/files[0],roles[2],2048,false,invalidImage),"Swapped DDS role accepted");negativeCases+=2;
        // 以下界限取自官方六型 cook 收據；半徑另以原 binary double hypot 獨立量測。
        constexpr std::array<Vec3,6> originalMinimum{{
            {-.0284300297498703f,-.0015644739614799619f,-.04656519740819931f},
            {-.01987099088728428f,-.0016869709361344576f,-.03346773236989975f},
            {-.03092743456363678f,-.0012624202063307166f,-.02224699966609478f},
            {-.02956516295671463f,-.0007196160149760544f,-.018236525356769562f},
            {-.05262836813926697f,-.003934997133910656f,-.04579240083694458f},
            {-.04104148596525192f,-.0055817291140556335f,-.04856731742620468f}}};
        constexpr std::array<Vec3,6> originalMaximum{{
            {.048856571316719055f,.12992630898952484f,.04249810427427292f},
            {.04561179503798485f,.1028880923986435f,.03269710764288902f},
            {.03326515480875969f,.06130361184477806f,.021210959181189537f},
            {.030160607770085335f,.02100224234163761f,.020272545516490936f},
            {.05096288397908211f,.21283993124961853f,.04426511004567146f},
            {.041281938552856445f,.18714646995067596f,.04611610248684883f}}};
        constexpr std::array<float,6> expectedRadius{.20057883247698116f,.182629490783911f,.13403240665764882f,
            .12202186799815858f,.21835748304264008f,.2226170655601135f};
        constexpr std::array<unsigned,6> expectedVertices{4585,4336,2736,849,5259,6258};
        constexpr std::array<unsigned,6> expectedTriangles{5896,5664,3592,1088,6984,8080};
        constexpr std::array<std::uintmax_t,6> expectedMeshBytes{199148,189392,119728,36844,231076,272200};
        unsigned vertices{},triangles{},rootVertices{},movingVertices{};
        for(unsigned variant=0;variant<6;++variant) {
            const auto path=root/"meshes"/NettleMeshName(variant);Require(std::filesystem::file_size(path)==expectedMeshBytes[variant],"Cooked nettle source byte count changed");
            auto loaded=LoadAevryndMeshFile(path);Require(bool(loaded),"Actual nettle mesh load failed");const auto original=loaded.mesh;const auto sourceHash=MeshHash(original);
            Require(original.vertices.size()==expectedVertices[variant]&&original.indices.size()==std::size_t(expectedTriangles[variant])*3&&
                NettleVertexCounts[variant]==expectedVertices[variant]&&NettleTriangleCounts[variant]==expectedTriangles[variant],"Actual six-type nettle topology changed");
            const float scale=NormalizeNettleAsset(loaded.mesh,variant);const auto repeat=CreateNettleAsset(original,variant);
            const auto bounds=MeasureNettleAssetBounds(loaded.mesh);
            Require(scale==4.f&&MeshHash(original)==sourceHash&&MeshHash(loaded.mesh)==MeshHash(repeat)&&loaded.mesh.indices==original.indices,"Source immutability or deterministic four-times author failed");
            // 官方 glTF 為右手座標；原 cook 已反射 Z，故原最小 Z 對應引擎最大 Z。
            const Vec3 nativeMinimum{originalMinimum[variant].x,originalMinimum[variant].y,-originalMaximum[variant].z};
            const Vec3 nativeMaximum{originalMaximum[variant].x,originalMaximum[variant].y,-originalMinimum[variant].z};
            Require(Length(bounds.minimum-nativeMinimum*4.f)<.0000002f&&Length(bounds.maximum-nativeMaximum*4.f)<.0000002f&&
                std::abs(bounds.radius-expectedRadius[variant])<.0000002f&&Same(bounds.minimumY,bounds.minimum.y)&&Same(bounds.maximumY,bounds.maximum.y),
                "Measured nettle full bounds differ from independent literals");
            unsigned fixedRoots{},movingTips{};bool hasUv=false;float measuredRadius{},minimumY=10.f,maximumY=-10.f;
            for(std::size_t index=0;index<original.vertices.size();++index) {
                const auto& a=original.vertices[index];const auto& b=loaded.mesh.vertices[index];
                Require(Same(b.x,a.x*4.f)&&Same(b.y,a.y*4.f)&&Same(b.z,a.z*4.f)&&Same(b.u,a.u)&&Same(b.v,a.v)&&
                    b.color==a.color&&(b.normal&0x00ffffffu)==(a.normal&0x00ffffffu),"Four-times scan altered UV/topology/XYZ normal/color or pivot");
                const auto normal=UnpackNormalSnorm8(b.normal);const float expected=1.f-std::clamp(a.y/originalMaximum[variant].y,0.f,1.f);
                Require(std::abs(Length({normal.x,normal.y,normal.z})-1.f)<.02f&&normal.w>=0.f&&normal.w<=1.f&&
                    std::abs(normal.w-expected)<.008f,"Nettle unit normal or inverse-height wind failed");
                if(a.y<=0.f){Require(normal.w==1.f,"Nettle root wind unlocked");++fixedRoots;}
                if(a.y>originalMaximum[variant].y*.95f){Require(normal.w<=.06f,"Nettle tip wind locked");++movingTips;}
                hasUv|=a.u!=0.f||a.v!=0.f;measuredRadius=(std::max)(measuredRadius,std::hypot(b.x,b.z));
                minimumY=(std::min)(minimumY,b.y);maximumY=(std::max)(maximumY,b.y);
            }
            Require(hasUv&&fixedRoots&&movingTips&&Same(measuredRadius,bounds.radius)&&Same(minimumY,bounds.minimumY)&&Same(maximumY,bounds.maximumY),
                "Actual root/tip/UV/full scan bounds not exercised");
            const auto reject=[&](Mesh3D corrupt,unsigned badVariant,const char* message) {
                const auto before=MeshHash(corrupt);bool rejected=false;
                try{(void)NormalizeNettleAsset(corrupt,badVariant);}catch(const std::invalid_argument&){rejected=true;}
                Require(rejected&&MeshHash(corrupt)==before,message);++negativeCases;
            };
            auto corrupt=original;corrupt.vertices[0].x=std::numeric_limits<float>::quiet_NaN();reject(std::move(corrupt),variant,"NaN position accepted or partial edit left");
            corrupt=original;corrupt.vertices[0].u=std::numeric_limits<float>::infinity();reject(std::move(corrupt),variant,"Infinite UV accepted or partial edit left");
            corrupt=original;corrupt.vertices[0].normal=0;reject(std::move(corrupt),variant,"Invalid XYZ normal accepted");
            corrupt=original;corrupt.indices[0]=static_cast<std::uint32_t>(original.vertices.size());reject(std::move(corrupt),variant,"Invalid source index accepted");
            corrupt=original;corrupt.indices.pop_back();reject(std::move(corrupt),variant,"Incomplete triangle list accepted");
            corrupt=original;for(auto& vertex:corrupt.vertices)vertex.y=0.f;reject(std::move(corrupt),variant,"Flat source accepted");
            corrupt=original;corrupt.vertices[0].x=(std::numeric_limits<float>::max)();reject(std::move(corrupt),variant,"Four-times overflow accepted or partial edit left");
            reject(original,6,"Out-of-range variant accepted");reject(original,(variant+1)%6,"Wrong exact variant accepted");
            vertices+=static_cast<unsigned>(loaded.mesh.vertices.size());triangles+=static_cast<unsigned>(loaded.mesh.indices.size()/3);
            rootVertices+=fixedRoots;movingVertices+=movingTips;
            std::cout<<"variant="<<variant<<" vertices="<<loaded.mesh.vertices.size()<<" triangles="<<loaded.mesh.indices.size()/3<<
                " radius="<<bounds.radius<<" minimum_y="<<bounds.minimumY<<" maximum_y="<<bounds.maximumY<<
                " fixed_roots="<<fixedRoots<<" moving_tips="<<movingTips<<" source_fingerprint="<<sourceHash<<" authored_fingerprint="<<MeshHash(loaded.mesh)<<'\n';
        }
        Require(vertices==24023&&triangles==31304&&NettleMeshName(6).empty()&&rootVertices&&movingVertices,"Complete six-type nettle population identity missing");
        std::cout<<"NETTLE_ASSET_CPU_PASS profile="<<NettleAssetProfile<<" variants=6 vertices="<<vertices<<" triangles="<<triangles<<
            " scale=4 sealed_DDS=3 negative_cases="<<negativeCases<<" world_integration=UNVERIFIED native_visual=UNVERIFIED\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<'\n';return 1;}
}
