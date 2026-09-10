#include "GameplayHeroPackageIdentity3D.h"
#include <iostream>
#include <fstream>
using namespace GameplayLab;
int main(int argc,char** argv){
    if(argc!=3)return 2;
    try{
        auto selected=SelectHeroPackage3D({1,"candidate7-mpfb-v1",argv[1]});
        if(selected.actualHeroId!="candidate7-mpfb-v1"||selected.identityReceiptJson.find("CPU_LOADED_NOT_RENDERED")==std::string::npos)return 3;
        unsigned rejected=0;
        for(unsigned mutation=0;mutation<4;++mutation){HeroPackageSelection3D request{1,"candidate7-mpfb-v1",argv[1]};if(mutation==0)request.schemaVersion=2;if(mutation==1)request.heroId="female-f2";if(mutation==2)request.heroId.push_back('\0');if(mutation==3)request.root/="__missing_hero_package__";try{(void)SelectHeroPackage3D(request);}catch(const std::exception&){++rejected;}}
        for(unsigned mutation=0;mutation<4;++mutation){auto package=selected.package;if(mutation==0)package.components[1].asset.model.skeletonCompatibilityId.value^=1;if(mutation==1)package.components[2].asset.model.skeleton.joints[47].bindTransform.translation.x+=.001f;if(mutation==2)package.components[3].asset.jointNames[47]="wrong_foot";if(mutation==3)package.components[0].sidecar.materials.pop_back();try{ValidateCandidate7ComponentIdentity3D(package);}catch(const std::exception&){++rejected;}}
        // 真實磁碟內容負例，不以假驗證callback冒充SHA gate。
        const auto fixture=std::filesystem::path(argv[2]).parent_path()/"hostile-files";
        if(std::filesystem::exists(fixture))throw std::runtime_error("Hostile output must be a fresh directory");
        const auto body=fixture/"face-detail-59/candidate7/body.aeskin",sidecar=fixture/"face-detail-59/body-materials.json";
        std::filesystem::create_directories(body.parent_path());
        std::filesystem::copy_file(std::filesystem::path(argv[1])/"face-detail-59/candidate7/body.aeskin",body);
        {std::ofstream corrupt(body,std::ios::binary|std::ios::app);corrupt.put('X');}
        try{(void)SelectHeroPackage3D({1,"candidate7-mpfb-v1",fixture});}catch(const std::exception& e){if(std::string(e.what())=="Hero pinned SHA256 mismatch")++rejected;}
        std::filesystem::copy_file(std::filesystem::path(argv[1])/"face-detail-59/candidate7/body.aeskin",body,std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(std::filesystem::path(argv[1])/"face-detail-59/body-materials.json",sidecar);
        {std::ofstream corrupt(sidecar,std::ios::binary|std::ios::app);corrupt.put(' ');}
        try{(void)SelectHeroPackage3D({1,"candidate7-mpfb-v1",fixture});}catch(const std::exception& e){if(std::string(e.what())=="Hero pinned SHA256 mismatch")++rejected;}
        if(rejected!=10)return 4;
        std::ofstream receipt(argv[2],std::ios::binary);receipt<<selected.identityReceiptJson<<'\n';receipt.flush();if(!receipt)return 5;
        std::cout<<"PASS actualHeroId="<<selected.actualHeroId<<" components=4 hashes=8 joints=53 hostile=10/10 stage=CPU_LOADED_NOT_RENDERED\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 10;}return 0;
}
