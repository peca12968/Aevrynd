#include <Aevrynd/CharacterShapeAsset3D.h>
#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/SkinnedMaterialSidecar3D.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Aevrynd/Serialization.h>
#include <objbase.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cmath>

using namespace Aevrynd;
namespace {
constexpr unsigned Width=900,Height=1200;
MaterialAlphaMode3D PreviewAlphaMode(SkinnedMaterialAlphaMode3D source){
    // GPU 蒙皮預覽目前只有 OPAQUE 與 MASK；來源的 BLEND 在診斷模式明確退回 OPAQUE，
    // 避免完全不透明的皮膚資產因匯入器殘留的 BLEND 標記而無法顯示。
    return source==SkinnedMaterialAlphaMode3D::Mask?MaterialAlphaMode3D::Mask:MaterialAlphaMode3D::Opaque;
}
bool SubmitPreviewCharacterPart(DrawList& draw,SkinnedMeshHandle3D mesh,SkinPaletteRange3D palette,
    Matrix4 transform,const Material3D& material,uint32_t firstIndex,uint32_t indexCount){
    return draw.SkinnedMeshInstance(mesh,palette,transform,material.tint,material.surface,
        material.textures,material.textureScale,material.useUv,material.materialClass,
        material.castsShadow,DirectionalShadowAllCascades3D,firstIndex,indexCount,
        material.shadingModel,material.alphaMode,material.alphaCutoff,material.doubleSided,
        {},Matrix4::Identity(),false,material.normalScale,material.baseTextureScale,material.detailTextureScale);
}
std::vector<std::byte> ReadBaseline(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    const auto size=file.tellg();
    if(!file||size<=0||size>64*1024*1024)throw std::runtime_error("baseline_read_failed");
    std::vector<std::byte> bytes(static_cast<size_t>(size));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("baseline_read_failed");
    return bytes;
}
LRESULT CALLBACK PreviewProc(HWND window,UINT message,WPARAM w,LPARAM l){
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,w,l);
}
struct WindowOwner {HWND value{};~WindowOwner(){if(value&&IsWindow(value))DestroyWindow(value);}};
bool IsPng(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);std::array<unsigned char,8> signature{};
    file.read(reinterpret_cast<char*>(signature.data()),signature.size());
    return file&&signature==std::array<unsigned char,8>{137,80,78,71,13,10,26,10}&&std::filesystem::file_size(path)>128;
}
}
int main(int argc,char** argv){
    // 必須給定目前輸入與全新輸出位置，缺少資產不能當成測試通過。
    if(argc<4){
        std::cerr<<"Usage: CharacterShapePreviewD3D12 baseline.aeskin shapes.acms fresh-output-directory [--interactive] [--face] [--materials] [--gpu-validation] [--weight target=value ...]\n";return 2;
    }
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(com)){std::cerr<<"com_initialize_failed\n";return 3;}
    int result=0;
    try{
        const auto argumentPath=[](const char* value){return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(value)));};
        const std::filesystem::path baselinePath=argumentPath(argv[1]),sidecarPath=argumentPath(argv[2]),outputPath=argumentPath(argv[3]);
        bool interactive=false,faceView=false,materialView=false,gpuValidation=false;
        std::filesystem::path materialSidecar;bool groundView=false;
        std::vector<std::filesystem::path> attachmentPaths;
        std::array<std::filesystem::path,4> attachmentMaterialSidecars{};
        bool relaxedPose=false,orbitSpecified=false;float initialOrbit=0;
        std::vector<CharacterShapeWeight3D> preset;
        for(int i=4;i<argc;++i){
            const std::string option=argv[i];
            // 固定鏡頭角度供正面／側面可重現比較，拒絕重複或非有限輸入。
            if(option=="--orbit"&&!orbitSpecified&&i+1<argc){
                const std::string number=argv[++i];size_t used=0;initialOrbit=std::stof(number,&used);
                if(used!=number.size()||!std::isfinite(initialOrbit)||std::abs(initialOrbit)>3.1415927f)throw std::runtime_error("invalid_orbit");
                orbitSpecified=true;continue;
            }
            if(option=="--relaxed"&&!relaxedPose){relaxedPose=true;continue;}
            if(option=="--gpu-validation"&&!gpuValidation){gpuValidation=true;continue;}
            if(option=="--attach"&&i+1<argc&&attachmentPaths.size()<4){attachmentPaths.push_back(argumentPath(argv[++i]));continue;}
            if(option=="--attachment-material-sidecar"&&i+1<argc){const std::string assignment=argv[++i];const auto equal=assignment.find('=');
                if(equal!=1||equal+1>=assignment.size()||assignment[0]<'0'||assignment[0]>'3')throw std::runtime_error("invalid_attachment_material_sidecar_assignment");
                const auto index=size_t(assignment[0]-'0');if(!attachmentMaterialSidecars[index].empty())throw std::runtime_error("duplicate_attachment_material_sidecar");
                attachmentMaterialSidecars[index]=argumentPath(assignment.c_str()+equal+1);continue;}
            if(option=="--ground"&&!groundView){groundView=true;continue;}
            if(option=="--interactive"&&!interactive){interactive=true;continue;}
            if(option=="--face"&&!faceView){faceView=true;continue;}
            if(option=="--materials"&&!materialView){materialView=true;continue;}
            if(option=="--material-sidecar"&&materialSidecar.empty()&&i+1<argc){materialSidecar=argumentPath(argv[++i]);materialView=true;continue;}
            if(option!="--weight"||i+1>=argc)throw std::runtime_error("invalid_or_duplicate_option: "+option);
            const std::string assignment=argv[++i];const auto equal=assignment.find('=');
            if(equal==std::string::npos||equal==0||equal+1==assignment.size())throw std::runtime_error("invalid_weight_assignment");
            const auto name=assignment.substr(0,equal),number=assignment.substr(equal+1);
            for(const auto& weight:preset)if(weight.name==name)throw std::runtime_error("duplicate_weight: "+name);
            size_t consumed=0;const float weight=std::stof(number,&consumed);
            if(consumed!=number.size()||!std::isfinite(weight))throw std::runtime_error("invalid_weight_value: "+name);
            preset.push_back({name,weight});
        }
        // CLI 與外部測試環境都能要求 GBV；只要任一路徑提出要求，就不得退回一般擷取。
        if(gpuValidation&&!SetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",L"1"))throw std::runtime_error("gpu_validation_environment_failed");
        wchar_t gpuValidationEnvironment[2]{};
        const DWORD gpuValidationLength=GetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",gpuValidationEnvironment,2);
        if(gpuValidationLength>1||(gpuValidationLength==1&&gpuValidationEnvironment[0]!=L'0'&&gpuValidationEnvironment[0]!=L'1'))throw std::runtime_error("gpu_validation_environment_invalid");
        const bool gpuValidationRequired=gpuValidationLength==1&&gpuValidationEnvironment[0]==L'1';
        for(size_t index=attachmentPaths.size();index<attachmentMaterialSidecars.size();++index)if(!attachmentMaterialSidecars[index].empty())throw std::runtime_error("attachment_material_sidecar_without_attachment");
        const auto baselineBytes=ReadBaseline(baselinePath);
        const auto loaded=LoadSkinnedModelAssetDetailed(baselineBytes);
        if(!loaded)throw std::runtime_error("baseline_decode_failed");
        std::vector<SkinnedModelAssetData> attachments;
        for(const auto& path:attachmentPaths){
            auto attachment=LoadSkinnedModelAssetDetailed(ReadBaseline(path));
            if(!attachment||attachment.asset.model.skeletonCompatibilityId!=loaded.asset.model.skeletonCompatibilityId||attachment.asset.jointNames!=loaded.asset.jointNames)throw std::runtime_error("attachment_skeleton_or_asset_invalid");
            if(attachment.asset.materials.size()!=attachment.asset.materialSlotNames.size())throw std::runtime_error("attachment_materials_missing");
            attachments.push_back(std::move(attachment.asset));
        }
        // 先驗證共同休息姿態的蒙皮重建，避免只有骨架名稱相同卻在實際變形時錯位。
        auto verifyBindSkin=[&](const SkinnedModel3D& model){
            AnimationPose3D pose;SampleAnimationClipInto(model.skeleton,{},0,pose);
            SkinningWorkspace3D workspace;Mesh3D result;
            if(!SkinModelInto(model,pose,workspace,result)||result.vertices.size()!=model.bindMesh.vertices.size())throw std::runtime_error("bind_skin_failed");
            float maximum=0;
            for(size_t index=0;index<result.vertices.size();++index){const auto& a=result.vertices[index];const auto& b=model.bindMesh.vertices[index];maximum=(std::max)(maximum,std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)));}
            if(maximum>0.0001f)throw std::runtime_error("bind_skin_position_mismatch");
            return maximum;
        };
        std::vector<float> bindErrors{verifyBindSkin(loaded.asset.model)};
        for(const auto& attachment:attachments)bindErrors.push_back(verifyBindSkin(attachment.model));
        AnimationPose3D characterPose;SampleAnimationClipInto(loaded.asset.model.skeleton,{},0,characterPose);
        if(relaxedPose){
            // 局部旋轉僅為候選姿勢；保留 bind 四元數並由共同骨架傳遞至衣物。
            for(const auto& name:{std::string("upperarm_l"),std::string("upperarm_r")}){
                const auto found=std::find(loaded.asset.jointNames.begin(),loaded.asset.jointNames.end(),name);
                if(found==loaded.asset.jointNames.end())throw std::runtime_error("pose_joint_missing");
                auto& transform=characterPose.localTransforms[size_t(found-loaded.asset.jointNames.begin())];
                const auto a=transform.usesQuaternion?transform.quaternion:QuaternionFromEuler3D(transform.rotation);
                // 左右上臂局部軸互為鏡像；由綁定姿勢向身側收臂。
                const auto b=QuaternionFromEuler3D({0,0,name=="upperarm_l"?-.45f:.45f});
                transform.quaternion=NormalizeQuaternion3D({a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z});transform.usesQuaternion=true;
            }
            BuildAnimationPoseGlobalMatrices3D(loaded.asset.model.skeleton,characterPose);
        }
        CharacterShapeAsset3D shapes;std::string error;
        if(!LoadCharacterShapeAssetFile3D(sidecarPath,baselineBytes,shapes,&error)||shapes.targets.empty())throw std::runtime_error("shape_load_failed: "+error);
        for(const auto& weight:preset){
            const auto target=std::find_if(shapes.targets.begin(),shapes.targets.end(),[&](const auto& item){return item.name==weight.name;});
            if(target==shapes.targets.end())throw std::runtime_error("unknown_weight_target: "+weight.name);
            if(weight.value<target->minimumWeight||weight.value>target->maximumWeight)throw std::runtime_error("weight_out_of_range: "+weight.name);
        }
        if(materialView){
            if(loaded.asset.submeshes.empty()||loaded.asset.materials.empty())throw std::runtime_error("material_metadata_missing");
            for(const auto& part:loaded.asset.submeshes)
                if(part.materialSlot>=loaded.asset.materials.size()||!part.indexCount||part.indexCount%3||
                   size_t(part.firstIndex)+part.indexCount>loaded.asset.model.bindMesh.indices.size())throw std::runtime_error("invalid_material_submesh");
        }
        if(std::filesystem::exists(outputPath)||!std::filesystem::create_directories(outputPath))throw std::runtime_error("output_must_be_new_directory");
        const auto& baseline=loaded.asset.model.bindMesh;
        // 形變保留原始蒙皮影響，身體與配件共用 GPU 骨架調色盤。
        std::ofstream receipt(outputPath/"preview-receipt.txt");
        if(!receipt)throw std::runtime_error("receipt_open_failed");
        // 影像必須綁定當次輸入，不以可變路徑當作版本證據。
        auto digestText=[](const std::vector<std::byte>& bytes){
            std::array<std::byte,32> digest{};
            if(!WorldDeltaSha2563D(bytes,digest))throw std::runtime_error("input_hash_failed");
            constexpr char hex[]="0123456789abcdef";std::string text;
            for(auto b:digest){const auto v=std::to_integer<unsigned>(b);text+=hex[v>>4];text+=hex[v&15];}
            return text;
        };
        receipt<<"BASELINE_SHA256="<<digestText(baselineBytes)<<"\nSIDECAR_SHA256="<<digestText(ReadBaseline(sidecarPath))<<"\n";
        for(const auto& path:attachmentPaths)receipt<<"ATTACHMENT_SHA256="<<digestText(ReadBaseline(path))<<" MODE=REST_SAME_SKELETON\n";
        for(size_t index=0;index<bindErrors.size();++index)receipt<<"BIND_SKIN_COMPONENT="<<index<<" MAX_ERROR_METERS="<<bindErrors[index]<<" TOLERANCE_METERS=0.0001\n";
        receipt<<"POSE="<<(relaxedPose?"RELAXED_CANDIDATE_UNVERIFIED":"BIND")<<"\n";
        receipt<<"STATUS="<<(materialView?"WIP_DIAGNOSTIC_MATERIAL_FACTORS":"WIP_DIAGNOSTIC_CLAY")
               <<"\nVALIDATION_SCOPE="<<(gpuValidationRequired?"GPU validation requested; every captured frame requires available/executed/passed and zero debug counters.":"Normal diagnostic only; zero counters do not prove debug layer or GBV execution.")
               <<"\nBASELINE="<<baselinePath.generic_string()<<"\nSIDECAR="<<sidecarPath.generic_string()
               <<"\nVERTICES="<<baseline.vertices.size()<<"\nINDICES="<<baseline.indices.size()<<"\nTARGETS="<<shapes.targets.size()<<"\n";
        WNDCLASSW cls{};cls.lpfnWndProc=PreviewProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"AevryndCharacterShapePreview";
        if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("window_class_failed");
        RECT rectangle{0,0,LONG(Width),LONG(Height)};AdjustWindowRect(&rectangle,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);
        WindowOwner window{CreateWindowW(cls.lpszClassName,L"Aevrynd WIP clay | Left/Right target | Up/Down weight | 0 neutral | A/D orbit | P capture",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,rectangle.right-rectangle.left,rectangle.bottom-rectangle.top,nullptr,nullptr,cls.hInstance,nullptr)};
        if(!window.value)throw std::runtime_error("window_create_failed");
        // GPU 驗證由渲染器既有的環境契約啟用；Initialize 的最後參數是 DLSS，不能混用。
        D3D12Renderer renderer;renderer.Initialize(window.value,Width,Height,Width,Height,false);renderer.SetQualitySettings({false,false,false,false});
        struct TextureOwner {D3D12Renderer& renderer;std::vector<MaterialTextureSetHandle3D> values;
            ~TextureOwner(){renderer.WaitIdle();for(auto handle:values)if(handle)renderer.DestroyMaterialTextureSet(handle);}} textureOwner{renderer,{}};
        std::vector<Material3D> bodyMaterials;
        for(const auto& source:loaded.asset.materials){
            Material3D material;material.tint=source.baseColorFactor;
            material.surface={source.roughnessFactor,source.metallicFactor,0,source.occlusionStrength};
            material.normalScale=source.normalScale;
            material.alphaMode=PreviewAlphaMode(source.alphaMode);
            material.alphaCutoff=source.alphaCutoff;material.doubleSided=source.doubleSided;
            material.textureScale=1.f;bodyMaterials.push_back(material);
        }
        std::vector<bool> absoluteDiagnosticRoughness(loaded.asset.materialSlotNames.size(),false);
        if(!materialSidecar.empty()){
            const auto materialRoot=std::filesystem::absolute(materialSidecar).parent_path();
            const auto documentBytes=ReadBaseline(materialSidecar);SerializationDocument document;SerializationError parseError;
            if(!SerializationDocument::Decode(std::string_view(reinterpret_cast<const char*>(documentBytes.data()),documentBytes.size()),document,&parseError))throw std::runtime_error("texture_manifest_decode_failed");
            if(!document.root.Contains("baselineSha256")){
                // 標準材質側車明確提供 Skin/Eye/Hair、UV 比例與 MASK 契約。
                auto sidecar=LoadSkinnedMaterialSidecarFile3D(materialSidecar,loaded.asset.materialSlotNames,materialRoot);
                if(!sidecar||!sidecar.PreserveSourcePbr(bodyMaterials))throw std::runtime_error("body_material_sidecar_load_failed");
                bodyMaterials.clear();
                for(auto& definition:sidecar.materials){
                    auto material=definition.material;material.textures=renderer.CreateMaterialTextureSet(definition.textures);
                    if(!material.textures)throw std::runtime_error("body_material_texture_upload_failed");
                    textureOwner.values.push_back(material.textures);bodyMaterials.push_back(material);
                }
                receipt<<"MATERIAL_SIDECAR_SHA256="<<digestText(documentBytes)<<"\nBASECOLOR_TEXTURE_MODE=SKINNED_MATERIAL_SIDECAR\n";
            }else{
            SerializationObject materials;std::string baselineHash;
            if(!document.root.Read("baselineSha256",baselineHash)||baselineHash!=digestText(baselineBytes)||!document.root.Read("materials",materials)||materials.FieldCount()!=loaded.asset.materialSlotNames.size())throw std::runtime_error("texture_manifest_binding_failed");
            receipt<<"MATERIAL_SIDECAR_SHA256="<<digestText(ReadBaseline(materialSidecar))<<"\nBASECOLOR_TEXTURE_MODE=SOURCE_UV_TEXTURES\n";
            for(const auto& name:loaded.asset.materialSlotNames){
                SerializationObject entry;bool enabled=false;
                if(!materials.Contains(name)||!materials.Read(name,entry)||!entry.Contains("enabled")||!entry.Read("enabled",enabled))throw std::runtime_error("texture_slot_missing");
                if(!enabled){textureOwner.values.push_back({});continue;}
                bool absoluteRoughness=false;
                if(entry.Contains("absoluteDiagnosticRoughness")&&!entry.Read("absoluteDiagnosticRoughness",absoluteRoughness))throw std::runtime_error("roughness_semantics_invalid");
                absoluteDiagnosticRoughness[textureOwner.values.size()]=absoluteRoughness;
                receipt<<"ROUGHNESS_SEMANTICS="<<(absoluteRoughness?"ABSOLUTE_DIAGNOSTIC_MAP":"SOURCE_FACTOR_TIMES_MAP")<<" SLOT="<<name<<"\n";
                std::string relative,sha;uint64_t bytes=0;
                if(!entry.Read("path",relative)||!entry.Read("sha256",sha)||!entry.Read("bytes",bytes)||!bytes)throw std::runtime_error("texture_integrity_missing");
                const auto path=std::filesystem::weakly_canonical(materialRoot/relative);
                const auto local=path.lexically_relative(std::filesystem::weakly_canonical(materialRoot));
                if(local.empty()||local.is_absolute()||*local.begin()=="..")throw std::runtime_error("texture_path_outside_root");
                const auto imageBytes=ReadBaseline(path);
                if(imageBytes.size()!=bytes||digestText(imageBytes)!=sha)throw std::runtime_error("texture_integrity_failed");
                MaterialTextureSetDefinition3D definition{};definition.baseColorPath=path;
                // 補充表面貼圖也必須有來源雜湊，不能靜默使用未宣告的替代檔案。
                auto surfacePath=[&](const char* field){
                    SerializationObject source;std::string sourceName,sourceHash;uint64_t sourceSize=0;
                    if(!entry.Read(field,source)||!source.Read("path",sourceName)||!source.Read("sha256",sourceHash)||!source.Read("bytes",sourceSize))throw std::runtime_error("surface_texture_metadata_missing");
                    const auto resolved=std::filesystem::weakly_canonical(materialRoot/sourceName);
                    const auto relativePath=resolved.lexically_relative(std::filesystem::weakly_canonical(materialRoot));
                    if(relativePath.empty()||relativePath.is_absolute()||*relativePath.begin()=="..")throw std::runtime_error("surface_texture_outside_root");
                    const auto data=ReadBaseline(resolved);
                    if(data.size()!=sourceSize||digestText(data)!=sourceHash)throw std::runtime_error("surface_texture_integrity_failed");
                    receipt<<"SURFACE_TEXTURE="<<field<<" SHA256="<<sourceHash<<"\n";
                    return resolved;
                };
                definition.roughnessPath=surfacePath("roughness");definition.normalPath=surfacePath("normal");
                const auto handle=renderer.CreateMaterialTextureSet(definition);
                if(!handle)throw std::runtime_error("basecolor_texture_upload_failed: "+name);
                textureOwner.values.push_back(handle);
                receipt<<"BASECOLOR_SLOT="<<name<<" SHA256="<<sha<<"\n";
            }
            for(size_t slot=0;slot<bodyMaterials.size();++slot){
                auto& material=bodyMaterials[slot];material.textures=textureOwner.values.at(slot);material.useUv=bool(material.textures);
                if(absoluteDiagnosticRoughness[slot])material.surface.roughness=1.f;
            }
            }
        }
        const auto bounds=baseline.Bounds();
        const bool zUp=bounds.extents.z>bounds.extents.y;
        const Vec3 rotation=zUp?Vec3{-1.570796327f,0,0}:Vec3{};
        const auto restMatrix=CreateTransformMatrix({},rotation,{1,1,1});
        float minimumHeight=std::numeric_limits<float>::max();
        for(const auto& vertex:baseline.vertices)minimumHeight=(std::min)(minimumHeight,TransformPoint(restMatrix,{vertex.x,vertex.y,vertex.z}).y);
        for(const auto& attachment:attachments)for(const auto& vertex:attachment.model.bindMesh.vertices)minimumHeight=(std::min)(minimumHeight,TransformPoint(restMatrix,{vertex.x,vertex.y,vertex.z}).y);
        const float groundOffset=groundView?-minimumHeight:0.f;
        const auto modelMatrix=CreateTransformMatrix({0,groundOffset,0},rotation,{1,1,1});
        receipt<<"GROUND_REFERENCE="<<(groundView?"EQUIPPED_BASELINE_MINIMUM":"DISABLED")<<" ROOT_OFFSET_Y="<<groundOffset<<"\n";
        Vec3 center=TransformPoint(modelMatrix,bounds.center);
        float radius=(std::max)(.05f,bounds.radius);
        // 同一人體包圍盒固定近景，不隨各 morph 重新框選而抵消差異。
        if(faceView){const float halfHeight=zUp?bounds.extents.z:bounds.extents.y;center.y+=halfHeight*.85f;radius=halfHeight*.24f;}
        const float distance=radius*3.3f;
        receipt<<"CAMERA_MODE="<<(faceView?"face":"body")<<"\n";
        receipt<<"MATERIAL_MODE="<<(materialView?"ASSET_FACTORS_DIAGNOSTIC":"CLAY_DIAGNOSTIC")
               <<"\nMATERIAL_LIMITATIONS=basecolor textures optional; supplied surface maps are diagnostic; attachment sidecars evaluate mask alpha but not blend, RGB emissive or unlit; not full material fidelity\n";
        if(materialView)for(size_t slot=0;slot<loaded.asset.materials.size();++slot){const auto& material=loaded.asset.materials[slot];
            receipt<<"MATERIAL_SLOT="<<slot<<" BASE_RGBA="<<material.baseColorFactor.r<<","<<material.baseColorFactor.g<<","<<material.baseColorFactor.b<<","<<material.baseColorFactor.a
                   <<" ROUGHNESS="<<material.roughnessFactor<<" METALLIC="<<material.metallicFactor<<" SOURCE_ALPHA_MODE="<<int(material.alphaMode)<<" SOURCE_UNLIT="<<material.unlit<<"\n";}
        int selected=-1;float angle=initialOrbit;unsigned captureId=0;
        // 所有局部資源使用同一形變結果；例外路徑也由擁有者等候 GPU 後釋放。
        struct PartMeshes {D3D12Renderer& renderer;std::vector<MeshHandle3D> values;
            void clear(){if(!values.empty()){renderer.WaitIdle();for(auto handle:values)renderer.DestroyMesh(handle);values.clear();}}
            ~PartMeshes(){clear();}} floor{renderer,{}};
        struct SkinnedMeshes {D3D12Renderer& renderer;std::vector<SkinnedMeshHandle3D> values;
            void clear(){if(!values.empty()){renderer.WaitIdle();for(auto handle:values)renderer.DestroySkinnedMesh(handle);values.clear();}}
            ~SkinnedMeshes(){clear();}} bodyMeshes{renderer,{}},attachmentMeshes{renderer,{}};
        const auto uploadSkinned=[&](const SkinnedModel3D& model){
            std::vector<SkinnedVertex3D> vertices;
            if(!BuildGpuSkinnedVertices(model,vertices))throw std::runtime_error("gpu_skinned_vertices_failed");
            const auto handle=renderer.CreateSkinnedMesh(vertices,model.bindMesh.indices,
                uint32_t(model.skeleton.joints.size()),model.skeletonCompatibilityId);
            if(!handle)throw std::runtime_error("skinned_mesh_upload_failed");
            return handle;
        };
        std::vector<std::vector<Material3D>> attachmentMaterialSets;
        attachmentMaterialSets.reserve(attachments.size());
        for(size_t attachmentIndex=0;attachmentIndex<attachments.size();++attachmentIndex){
            const auto& attachment=attachments[attachmentIndex];std::vector<Material3D> materials;
            if(attachmentMaterialSidecars[attachmentIndex].empty()){
                materials.reserve(attachment.materials.size());
                for(const auto& source:attachment.materials){Material3D material;material.tint=source.baseColorFactor;
                    material.surface={source.roughnessFactor,source.metallicFactor,0,source.occlusionStrength};material.normalScale=source.normalScale;
                    material.alphaMode=PreviewAlphaMode(source.alphaMode);
                    material.alphaCutoff=source.alphaCutoff;material.doubleSided=source.doubleSided;materials.push_back(material);}
            }else{
                const auto attachmentSidecarPath=std::filesystem::absolute(attachmentMaterialSidecars[attachmentIndex]);
                auto sidecar=LoadSkinnedMaterialSidecarFile3D(attachmentSidecarPath,attachment.materialSlotNames,attachmentSidecarPath.parent_path());
                if(!sidecar||sidecar.materials.size()!=attachment.materialSlotNames.size())throw std::runtime_error("attachment_material_sidecar_load_failed");
                for(auto& definition:sidecar.materials){auto material=definition.material;material.textures=renderer.CreateMaterialTextureSet(definition.textures);
                    if(!material.textures)throw std::runtime_error("attachment_material_texture_upload_failed");textureOwner.values.push_back(material.textures);materials.push_back(material);}
                receipt<<"ATTACHMENT_MATERIAL_SIDECAR_SHA256="<<digestText(ReadBaseline(attachmentSidecarPath))<<" ATTACHMENT="<<attachmentIndex<<"\n";
            }
            attachmentMaterialSets.push_back(std::move(materials));
        }
        for(const auto& attachment:attachments)attachmentMeshes.values.push_back(uploadSkinned(attachment.model));
        if(groundView){const auto geometry=Mesh3D::CreateUnitBox();const auto handle=renderer.CreateMesh(geometry.vertices,geometry.indices);if(!handle)throw std::runtime_error("ground_upload_failed");floor.values.push_back(handle);}
        std::vector<CharacterShapeWeight3D> weights;
        auto setSelected=[&](float value){
            const auto& name=shapes.targets[size_t(selected)].name;
            for(auto& weight:weights)if(weight.name==name){weight.value=value;return;}
            weights.push_back({name,value});
        };
        auto selectedValue=[&](){
            for(const auto& weight:weights)if(weight.name==shapes.targets[size_t(selected)].name)return weight.value;
            return 0.f;
        };
        auto rebuild=[&](){
            CharacterShapeResult3D shaped;
            if(!ApplyCharacterShape3D(baseline,shapes.targets,weights,shaped,&error))throw std::runtime_error("shape_apply_failed: "+error);
            for(auto& vertex:shaped.mesh.vertices)vertex.color=0xffffffffu;
            auto model=loaded.asset.model;model.bindMesh=std::move(shaped.mesh);
            // 形變後的法線與位置已改變；清除來源切線，讓 GPU 頂點資料依新幾何與 UV 重建切線。
            model.tangents.clear();
            const auto next=uploadSkinned(model);
            bodyMeshes.clear();bodyMeshes.values.push_back(next);
        };
        SkinningWorkspace3D paletteWorkspace;
        auto drawFrame=[&](){
            Camera3D camera{{center.x+std::sin(angle)*distance,center.y+radius*.04f,center.z-std::cos(angle)*distance},center,{0,1,0},{}};
            camera.projection.nearPlane=(std::max)(.001f,radius*.01f);camera.projection.farPlane=distance+radius*8;
            DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(camera,float(Width)/Height));draw.SetBackfaceCulling(false);
            // 著色器透過大氣常數取得視點；關閉霧仍須傳入實際相機位置，避免錯誤拒絕正面光照。
            Atmosphere3D atmosphere{};atmosphere.cameraPosition=camera.position;atmosphere.enabled=false;draw.SetAtmosphere(atmosphere);
            // 光線方向指向場景；相機位於負 Z 側，因此正 Z 光線才由角色正面入射。
            draw.SetDirectionalLight({{-.55f,-.72f,.42f},.28f,{1.f,.93f,.86f,1},1.7f});
            // 相機旁的實體柔和點光提供眼睛反光與髮絲高光；不是寫入貼圖的假亮點。
            draw.AddPointLight({{camera.position.x+.035f,camera.position.y+.045f,camera.position.z},.65f,{1.f,.96f,.92f,1.f},.28f});
            if(groundView&&!draw.MeshInstance(floor.values.front(),CreateTransformMatrix({0,-.025f,0},{},{3,.05f,3}),{.18f,.22f,.25f,1},{1,0,0,1}))throw std::runtime_error("ground_submit_failed");
            const auto palette=AddSkinPaletteFromPose(draw,loaded.asset.model,characterPose,paletteWorkspace);
            if(!palette)throw std::runtime_error("character_palette_failed");
            for(size_t index=0;index<attachments.size();++index)for(const auto& part:attachments[index].submeshes){
                const auto& material=attachmentMaterialSets[index].at(part.materialSlot);
                if(!SubmitPreviewCharacterPart(draw,attachmentMeshes.values[index],palette,modelMatrix,material,part.firstIndex,part.indexCount))throw std::runtime_error("attachment_submit_failed");
            }
            if(materialView){
                for(const auto& part:loaded.asset.submeshes)
                    if(!SubmitPreviewCharacterPart(draw,bodyMeshes.values.front(),palette,modelMatrix,bodyMaterials.at(part.materialSlot),part.firstIndex,part.indexCount))throw std::runtime_error("material_mesh_submit_failed");
            }else{
                Material3D clay;clay.tint={.62f,.56f,.49f,1};clay.surface={.78f,0,0,1};
                if(!SubmitPreviewCharacterPart(draw,bodyMeshes.values.front(),palette,modelMatrix,clay,0,uint32_t(baseline.indices.size())))throw std::runtime_error("mesh_submit_failed");
            }
            renderer.Render(draw,{.025f,.032f,.044f,1});
        };
        auto capture=[&](){
            const auto path=outputPath/("capture-"+std::to_string(captureId++)+".png");
            for(int i=0;i<3;++i)drawFrame();
            if(!renderer.CaptureNextFrame(path))throw std::runtime_error("capture_request_failed");
            drawFrame();renderer.WaitIdle();const auto stats=renderer.GetStats();
            const bool pngValid=IsPng(path);
            const bool diagnosticsPass=pngValid&&!stats.debugErrorCount&&!stats.debugWarningCount&&stats.lastSubmittedSkinnedInstances&&
                (!gpuValidationRequired||(stats.gpuValidationRequested&&stats.gpuValidationAvailable&&stats.gpuValidationExecuted&&stats.gpuValidationPassed));
            receipt<<"CAPTURE="<<path.filename().string()<<" MODE="<<(weights.empty()?"neutral":"combined")<<" ORBIT_RADIANS="<<angle
                   <<" PNG_VALID="<<pngValid<<" DIAGNOSTICS_PASS="<<diagnosticsPass
                   <<" GPU_ERRORS="<<stats.debugErrorCount<<" GPU_WARNINGS="<<stats.debugWarningCount
                   <<" SKINNED_INSTANCES="<<stats.lastSubmittedSkinnedInstances<<" SKIN_JOINTS="<<stats.lastUploadedSkinJoints
                   <<" SKIN="<<stats.lastSubmittedSkinShadedInstances<<" HAIR="<<stats.lastSubmittedHairShadedInstances
                   <<" EYE="<<stats.lastSubmittedEyeShadedInstances<<" MASK="<<stats.lastSubmittedMaskedSkinnedInstances
                   <<" DEBUG_LAYER="<<stats.debugLayerEnabled<<" GBV_REQUESTED="<<stats.gpuValidationRequested
                   <<" GBV_AVAILABLE="<<stats.gpuValidationAvailable<<" GBV_EXECUTED="<<stats.gpuValidationExecuted
                   <<" GBV_PASSED="<<stats.gpuValidationPassed<<"\n";
            receipt.flush();
            if(!diagnosticsPass)throw std::runtime_error("capture_or_gpu_diagnostic_failed");
            // 每張影像記錄全部目標，包含未指定的零值，讓組合與中立對照可重現。
            receipt<<std::setprecision(std::numeric_limits<float>::max_digits10);
            for(const auto& target:shapes.targets){float value=0;for(const auto& weight:weights)if(weight.name==target.name)value=weight.value;
                receipt<<"CAPTURE_WEIGHT="<<path.filename().string()<<" TARGET="<<target.name<<" VALUE="<<value<<"\n";}
            receipt.flush();
        };
        rebuild();capture();
        if(!preset.empty()){weights=preset;rebuild();capture();}
        if(!interactive){
            if(preset.empty())for(size_t index=0;index<shapes.targets.size();++index){weights={{shapes.targets[index].name,shapes.targets[index].maximumWeight}};rebuild();capture();}
        }else{
            ShowWindow(window.value,SW_SHOWNORMAL);drawFrame();
            MSG message{};int received{};
            while((received=GetMessageW(&message,nullptr,0,0))>0){
                bool changed=false,orbit=false;
                if(message.message==WM_KEYDOWN){
                    switch(message.wParam){
                    case VK_ESCAPE:DestroyWindow(window.value);break;
                    case '0':selected=-1;weights.clear();changed=true;break;
                    case 'R':weights=preset;changed=true;break;
                    case VK_RIGHT:selected=(selected+1)%int(shapes.targets.size());break;
                    case VK_LEFT:selected=selected<=0?int(shapes.targets.size())-1:selected-1;break;
                    case VK_UP:case VK_DOWN:if(selected>=0){const auto& target=shapes.targets[size_t(selected)];setSelected(std::clamp(selectedValue()+(message.wParam==VK_UP?.1f:-.1f),target.minimumWeight,target.maximumWeight));changed=true;}break;
                    case 'A':angle-=.15f;orbit=true;break;
                    case 'D':angle+=.15f;orbit=true;break;
                    case 'P':capture();break;
                    }
                }
                if(changed)rebuild();
                if(message.message==WM_KEYDOWN){
                    const std::string title="Aevrynd WIP clay | target="+(selected<0?std::string("none"):shapes.targets[size_t(selected)].name)+
                        " value="+(selected<0?std::string("0"):std::to_string(selectedValue()))+" | Up/Down edit | 0 neutral | R preset | P capture";
                    SetWindowTextA(window.value,title.c_str());
                }
                if(changed||orbit)drawFrame();
                TranslateMessage(&message);DispatchMessageW(&message);
            }
            if(received<0)throw std::runtime_error("message_loop_failed");
        }
        renderer.WaitIdle();bodyMeshes.clear();
        receipt<<"COMPLETED=1\n";
        std::cout<<"CharacterShapePreviewD3D12 WIP captures="<<captureId<<" vertices="<<baseline.vertices.size()<<" targets="<<shapes.targets.size()<<"\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";result=1;}
    CoUninitialize();return result;
}
