#include "GameplayClipSegmentVisibility3D.h"
#include <iostream>
#include <limits>

using Aevrynd::Vec4;
using GameplayLab::ClassifyGameplayClipSegmentVisibility3D;
using Result=GameplayLab::GameplayClipSegmentVisibility3D;

int main(){
    struct Case {const char* name;Vec4 first,second;Result expected;};
    const float maximum=std::numeric_limits<float>::max();
    const Case cases[]={
        {"inside",{0,0,.5f,1},{.5f,.5f,.8f,1},Result::Intersects},
        {"outsideLeft",{-3,0,.5f,1},{-2,0,.5f,1},Result::Outside},
        {"bothOutsideCrossViewport",{-2,0,.5f,1},{2,0,.5f,1},Result::Intersects},
        {"bothOutsideMissCorner",{.9f,2,.5f,1},{2,.9f,.5f,1},Result::Outside},
        {"touchCorner",{0,2,.5f,1},{2,0,.5f,1},Result::Intersects},
        {"parallelOutside",{-2,2,.5f,1},{2,2,.5f,1},Result::Outside},
        {"parallelOnBoundary",{-2,1,.5f,1},{2,1,.5f,1},Result::Intersects},
        {"nearPlaneCross",{0,0,-1,1},{0,0,.5f,1},Result::Intersects},
        {"nearPlaneTouch",{0,0,-1,1},{0,0,0,1},Result::Intersects},
        {"beforeNear",{0,0,-2,1},{0,0,-1,1},Result::Outside},
        {"beyondFar",{0,0,2,1},{0,0,3,1},Result::Outside},
        {"farPlaneTouch",{0,0,2,1},{0,0,1,1},Result::Intersects},
        {"completelyBehind",{0,0,-.5f,-1},{0,0,-1,-2},Result::Outside},
        {"crossNegativeW",{0,0,-.5f,-1},{0,0,.5f,1},Result::Intersects},
        {"crossNegativeWMiss",{4,0,-.5f,-1},{4,0,.5f,1},Result::Outside},
        {"pointInside",{0,0,.5f,1},{0,0,.5f,1},Result::Intersects},
        {"pointOutside",{2,0,.5f,1},{2,0,.5f,1},Result::Outside},
        {"homogeneousOrigin",{0,0,0,0},{0,0,0,0},Result::Intersects},
        {"zeroWNonOrigin",{1,0,0,0},{1,0,0,0},Result::Outside},
        {"largeFiniteInside",{maximum,0,0,maximum},{0,maximum,maximum,maximum},Result::Intersects},
        {"nonUnitWCross",{-4,0,1,2},{4,0,1,2},Result::Intersects}
    };
    unsigned checked{};
    for(const auto& item:cases){
        if(ClassifyGameplayClipSegmentVisibility3D(item.first,item.second)!=item.expected||
           ClassifyGameplayClipSegmentVisibility3D(item.second,item.first)!=item.expected){
            std::cerr<<"clip segment failed: "<<item.name<<'\n';return 1;
        }
        ++checked;
    }
    // 每個分量、兩個端點都驗證非有限輸入；不可誤報為幾何不可見。
    for(const float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        for(unsigned component=0;component<4;++component){
            Vec4 value{0,0,.5f,1};
            switch(component){case 0:value.x=invalid;break;case 1:value.y=invalid;break;case 2:value.z=invalid;break;default:value.w=invalid;break;}
            if(ClassifyGameplayClipSegmentVisibility3D(value,{0,0,.5f,1})!=Result::Invalid||
               ClassifyGameplayClipSegmentVisibility3D({0,0,.5f,1},value)!=Result::Invalid)return 2;
            ++checked;
        }
    }
    std::cout<<"clipSegment cases="<<checked<<" endpointOrderChecked=1 primitive=segment triangleCoverage=unmeasured gpuMask=unmeasured\n";
    return 0;
}
