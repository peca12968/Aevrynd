#include "MistwatchCapturePresentation.h"
#include <array>
#include <iostream>
#include <stdexcept>

int main(){
    try{
        unsigned negatives{};
        const auto reject=[&](std::wstring_view option,const std::array<bool,7>& flags){
            try{(void)Mistwatch::SelectCaptureBenchmarkPresentation(option,flags[0],flags[1],flags[2],flags[3],flags[4],flags[5],flags[6]);}
            catch(const std::runtime_error&){++negatives;return;}
            throw std::runtime_error("Unqualified capture presentation accepted");
        };
        const std::array<bool,7> valid{true,true,true,true,false,false,false};
        if(!Mistwatch::SelectCaptureBenchmarkPresentation(L"offscreen-unsynchronized",true,true,true,true,false,false,false))
            throw std::runtime_error("Measured static GPU benchmark rejected");
        for(std::size_t i=0;i<valid.size();++i){auto flags=valid;flags[i]=!flags[i];reject(L"offscreen-unsynchronized",flags);}
        reject(L"unsynchronized",valid);reject(L"offscreen",valid);reject(L"offscreen-unsynchronized-extra",valid);
        // 沒有明確選項時不變更既有mode；舊replay guard對capture仍必須拒絕。
        if(Mistwatch::SelectCaptureBenchmarkPresentation({},true,true,true,true,false,false,false))throw std::runtime_error("Implicit presentation override");
        try{(void)Aevrynd::DiagnosticWindow3D::SelectMode(L"offscreen-noactivate",false,true,true);}
        catch(const std::runtime_error&){++negatives;}
        if(negatives!=11)throw std::runtime_error("Legacy window guard changed");
        const std::array<bool,8> staticFlags{true,true,true,false,false,false,false,false};
        for(std::size_t i=0;i<staticFlags.size();++i){
            auto flags=staticFlags;flags[i]=!flags[i];
            if(Mistwatch::CaptureBenchmarkRemainsStatic(flags[0],flags[1],flags[2],flags[3],flags[4],flags[5],flags[6],flags[7]))
                throw std::runtime_error("Dynamic or unqualified scene accepted as static benchmark");
            ++negatives;
        }
        if(!Mistwatch::CaptureBenchmarkRemainsStatic(true,true,true,false,false,false,false,false))throw std::runtime_error("Static scene rejected");
        std::cout<<"CAPTURE_PRESENTATION_CPU_PASS negatives="<<negatives<<" defaults_unchanged=1\n";return 0;
    }catch(const std::exception& error){std::cerr<<"CAPTURE_PRESENTATION_CPU_FAIL "<<error.what()<<'\n';return 1;}
}
