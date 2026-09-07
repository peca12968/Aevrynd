#include <Aevrynd/CharacterShapeAsset3D.h>
#include <Aevrynd/SkinnedModelAsset.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;
std::vector<std::byte> Read(const char* path){std::ifstream file(path,std::ios::binary|std::ios::ate);const auto size=file.tellg();if(!file||size<=0||size>64*1024*1024)throw std::runtime_error("fixture_read");std::vector<std::byte> data(static_cast<size_t>(size));file.seekg(0);if(!file.read(reinterpret_cast<char*>(data.data()),size))throw std::runtime_error("fixture_read");return data;}
int main(int argc,char** argv){
    try{
        if(argc!=3)throw std::runtime_error("Expected sidecar and baseline fixture paths");
        const auto source=Read(argv[1]),baseline=Read(argv[2]);CharacterShapeAsset3D output;std::string error;
        if(!LoadCharacterShapeAsset3D(source,baseline,output,&error)||output.targets.empty())throw std::runtime_error(error);
        const auto name=output.targets[0].name;const auto count=output.targets.size();
        const auto model=LoadSkinnedModelAssetDetailed(baseline);
        if(!model)throw std::runtime_error("baseline_decode");
        CharacterShapeResult3D shaped;
        if(!ApplyCharacterShape3D(model.asset.model.bindMesh,output.targets,{},shaped,&error))throw std::runtime_error(error);
        // 使用正式載入器輸出的形變，不以測試專用解析器替代端到端路徑。
        for(const auto& target:output.targets){
            for(const float value:{target.minimumWeight,target.maximumWeight}){
                const CharacterShapeWeight3D weight{target.name,value};
                if(!ApplyCharacterShape3D(model.asset.model.bindMesh,output.targets,std::span(&weight,1),shaped,&error))throw std::runtime_error(target.name+":"+error);
            }
        }
        auto reject=[&](std::vector<std::byte> broken){if(LoadCharacterShapeAsset3D(broken,baseline,output,&error)||output.targets.size()!=count||output.targets[0].name!=name)throw std::runtime_error("Atomic reject failed");};
        auto broken=source;broken.pop_back();reject(broken);
        broken=source;broken[8]^=std::byte{1};reject(broken);
        broken=source;broken[40]=std::byte{255};broken[41]=std::byte{255};broken[42]=std::byte{255};broken[43]=std::byte{127};reject(broken);
        broken=source;broken.push_back(std::byte{0});reject(broken);
        broken=source;const size_t n=broken.size();broken[n-4]=std::byte{0};broken[n-3]=std::byte{0};broken[n-2]=std::byte{192};broken[n-1]=std::byte{127};reject(broken);
        std::cout<<"CharacterShapeAsset3D passed targets="<<count<<"\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
