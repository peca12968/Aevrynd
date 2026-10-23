#include "MistwatchErodedBankConstruction.h"
#include "MistwatchBearingBridge.h"
#include "MistwatchWestCliffRelief.h"
#include <bit>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc,char** argv) {
    try {
        if(argc!=3&&argc!=4)throw std::runtime_error("EXACT_CURRENT_FIELD_AND_GUARDS_REQUIRED");
        Aevrynd::HeightFieldSettings3D settings;settings.width=201;settings.depth=241;settings.cellSize=.5f;settings.origin={-50,0,-20};
        Aevrynd::HeightField3D original(settings),coherent(settings);
        std::ifstream input(argv[1]);std::string line;std::getline(input,line);
        if(line!="x\tz\tbefore\tafter")throw std::runtime_error("FIELD_SCHEMA");
        unsigned rows{};while(std::getline(input,line)){
            std::istringstream row(line);float x,z,before,after;std::string extra;
            if(!(row>>x>>z>>before>>after)||(row>>extra)||rows>=48441)throw std::runtime_error("FIELD_ROW");
            const unsigned ix=rows%201,iz=rows/201;
            if(x!=-50.f+float(ix)*.5f||z!=-20.f+float(iz)*.5f||!std::isfinite(before)||!std::isfinite(after))throw std::runtime_error("FIELD_IDENTITY_OR_FINITE");
            if(!original.SetHeight(ix,iz,before)||!coherent.SetHeight(ix,iz,after))throw std::runtime_error("FIELD_WRITE");++rows;
        }
        original.RecalculateRange();coherent.RecalculateRange();
        if(rows!=48441)throw std::runtime_error("INCOMPLETE_FIELD");
        if(Mistwatch::WestCliffRelief::Fingerprint(coherent)!=10513288999544626110ull)throw std::runtime_error("COHERENT_SOURCE_FINGERPRINT_CHANGED");
        std::vector<Mistwatch::RiverbankConstruction::Guard> guards;
        std::ifstream guardFile(argv[2]);std::getline(guardFile,line);if(line!="x\tz\tex\tez")throw std::runtime_error("GUARD_SCHEMA");
        while(std::getline(guardFile,line)){Mistwatch::RiverbankConstruction::Guard g;std::istringstream row(line);std::string extra;if(!(row>>g.x>>g.z>>g.ex>>g.ez)||(row>>extra))throw std::runtime_error("GUARD_ROW");guards.push_back(g);}
        if(guards.size()!=223)throw std::runtime_error("ACTUAL223_GUARDS_REQUIRED");
        const auto result=Mistwatch::ErodedBank::Build(original,coherent,guards);
        unsigned protectedChecks{},outside{},deep{},changed{};float maxOriginalDelta{};
        for(unsigned iz=0;iz<241;++iz)for(unsigned ix=0;ix<201;++ix){
            const auto p=coherent.SamplePosition(ix,iz);const float y=result.field.Height(ix,iz);
            const bool same=std::bit_cast<unsigned>(p.y)==std::bit_cast<unsigned>(y);
            if(!std::isfinite(y)||y>p.y||std::abs(y-original.Height(ix,iz))>4.5f)throw std::runtime_error("FINITE_OR_ORIGINAL_BOUND");
            maxOriginalDelta=(std::max)(maxOriginalDelta,std::abs(y-original.Height(ix,iz)));
            if(Mistwatch::ErodedBank::Domain(p.x,p.z)==0){if(!same)throw std::runtime_error("OUTSIDE_DOMAIN_CHANGED");++outside;}
            if(p.y<=Mistwatch::ErodedBank::FootHeight){if(!same)throw std::runtime_error("ACTUAL_WATERLINE_AND_SUBMERGED_SAMPLES_CHANGED");++deep;}
            for(const auto& g:guards)if(std::abs(p.x-g.x)<=g.ex&&std::abs(p.z-g.z)<=g.ez){if(!same)throw std::runtime_error("PROTECTED_ACTUAL_ROOT_OR_SOLID_CHANGED");++protectedChecks;}
            changed+=!same;
        }
        const auto oldBridge=Mistwatch::BearingBridge::Build(coherent),newBridge=Mistwatch::BearingBridge::Build(result.field);
        if(oldBridge.parts.size()!=newBridge.parts.size()||oldBridge.additions.size()!=newBridge.additions.size()||oldBridge.beds!=newBridge.beds)throw std::runtime_error("BRIDGE_FOUNDATION_CHANGED");
        for(std::size_t i=0;i<oldBridge.parts.size();++i){const auto& a=oldBridge.parts[i];const auto& b=newBridge.parts[i];if(std::memcmp(&a.center,&b.center,sizeof(a.center))||std::memcmp(&a.size,&b.size,sizeof(a.size))||a.role!=b.role||a.tone!=b.tone)throw std::runtime_error("BRIDGE_PART_CHANGED");}
        if(changed!=result.changed)throw std::runtime_error("CHANGED_COUNT");
        bool rejected=false;try{auto bad=guards;bad[0].ex=0;static_cast<void>(Mistwatch::ErodedBank::Build(original,coherent,bad));}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("INVALID_GUARD_ACCEPTED");
        if(argc==4){if(std::filesystem::exists(argv[3]))throw std::runtime_error("EXPECTED_FIELD_OUTPUT_ALREADY_EXISTS");std::ofstream out(argv[3],std::ios::binary);for(unsigned iz=0;iz<241;++iz)for(unsigned ix=0;ix<201;++ix){const float value=result.field.Height(ix,iz);out.write(reinterpret_cast<const char*>(&value),sizeof(value));}out.close();if(!out)throw std::runtime_error("EXPECTED_FIELD_EXPORT");}
        std::cout<<std::setprecision(9)<<"BANK_SLOPE_CONTRACT_PASS changed="<<changed<<" lowering="<<result.maximumLowering<<" max_original_delta="<<maxOriginalDelta<<" outside="<<outside<<" wet_samples_unchanged="<<deep<<" protected_grid_checks="<<protectedChecks<<" bridge_parts="<<newBridge.parts.size()<<" bridge_obstacles="<<newBridge.additions.size()<<'\n';
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
