// 以生產CC的同一私有實作執行兩組原始案例；不新增公開測試介面。
#include <array>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

#ifndef PROJECTION_PROFILE_NAME
#define PROJECTION_PROFILE_NAME "unspecified"
#endif
#ifndef HAS_REPAIR
#error HAS_REPAIR must be enabled for the permanent projection regression
#endif
static_assert(HAS_REPAIR==1);

// 每個執行檔只編入一次CC；不得另連共用CC object或再列CC.cpp為來源。
#include "../src/CharacterController3D.cpp"
#include "support/dynamic-contact/ProjectionResidualCases.h"
#include "support/dynamic-contact/MixedPlaneProjectionCases.h"

int main(){
    if(const int result=RunProjectionResidualCases();result!=0)return result;
    if(const int result=RunMixedPlaneProjectionCases();result!=0)return result;
    if(ProjectionResidualTrial::cases!=25||ProjectionResidualTrial::checks!=106||
       MixedPlaneProjectionTrial::cases!=24||MixedPlaneProjectionTrial::checks!=273){
        std::cerr<<"CHARACTER_PROJECTION_RESIDUAL_FIRST_DIVERGENCE=EXACT49_CASE_379_CHECK_INVENTORY\n";
        return 1;
    }
    std::cout<<"CHARACTER_PROJECTION_RESIDUAL_CPU_PASS cases=49 checks=379 residual_cases=25 residual_checks=106 multiplane_cases=24 multiplane_checks=273 profile="
        <<PROJECTION_PROFILE_NAME<<" full_controller=0 main=0 GPU=0\n";
    return 0;
}
