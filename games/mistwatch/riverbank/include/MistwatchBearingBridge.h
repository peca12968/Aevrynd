#pragma once
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/Mesh3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace Mistwatch::BearingBridge {
enum class Role : unsigned { Plank, Girder, Bearing, GuardPost, GuardRail, Footing, Pier };
struct Part { Aevrynd::Vec3 center,size; Role role; float tone; };
struct Model {
    std::vector<Part> parts;
    std::vector<Aevrynd::BoxCollider3D> additions;
    std::array<std::array<float,3>,4> beds{};
};
// Deck remains x[-2,2], z[32,40], top y=0. The original closed walking
// proxy stays intact; new rails, deep girders and terrain-founded piers have
// their own exact axis-aligned finite-volume contact boxes.
inline Model Build(const Aevrynd::HeightField3D& field) {
    Model out;
    const auto part=[&](Aevrynd::Vec3 c,Aevrynd::Vec3 size,Role role,float tone,bool collision=true){
        for(float v:{c.x,c.y,c.z,size.x,size.y,size.z,tone})
            if(!std::isfinite(v))throw std::runtime_error("BRIDGE_NONFINITE_PART");
        if(size.x<=0||size.y<=0||size.z<=0)throw std::runtime_error("BRIDGE_NONPOSITIVE_PART");
        out.parts.push_back({c,size,role,tone});
        if(collision)out.additions.push_back({c,size*.5f});
    };
    for(unsigned i=0;i<32;++i){
        const float low=32.f+float(i)*.25f+(i?.003f:0.f);
        const float high=32.f+float(i+1)*.25f-(i<31?.003f:0.f);
        part({0,-.07f,(low+high)*.5f},{4,.14f,high-low},Role::Plank,.88f+float((i*17u+7u)%11u)*.012f,false);
    }
    for(float x:{-1.4f,0.f,1.4f})part({x,-.50f,36},{.26f,.72f,8.5f},Role::Girder,.74f);
    for(float z:{34.5f,37.5f})part({0,-1.01f,z},{3.5f,.30f,.34f},Role::Bearing,.69f);
    for(float x:{-1.90f,1.90f}){
        for(float z:{32.2f,34.75f,37.25f,39.8f})part({x,.52f,z},{.17f,1.32f,.17f},Role::GuardPost,.78f);
        for(float y:{.49f,1.08f})part({x,y,36},{.14f,.12f,8.f},Role::GuardRail,.84f);
    }
    unsigned bed{};
    for(float z:{34.5f,37.5f})for(float x:{-1.4f,1.4f}){
        float lo=1.e6f,hi=-1.e6f;
        for(float dz:{-.40f,0.f,.40f})for(float dx:{-.40f,0.f,.40f}){
            if(!field.Contains(x+dx,z+dz))throw std::runtime_error("BRIDGE_FOUNDATION_OUTSIDE_TERRAIN");
            const float h=field.SampleHeight(x+dx,z+dz);lo=(std::min)(lo,h);hi=(std::max)(hi,h);
        }
        // Footing deliberately embeds below every sampled bed corner, unlike a
        // floating visual pad. Its top is above the highest sampled ground.
        const float bottom=lo-.25f,top=hi+.18f,head=-1.12f;
        if(!std::isfinite(top)||top>=head-.5f)throw std::runtime_error("BRIDGE_BED_TOO_HIGH_FOR_THIS_CONSTRUCTION");
        out.beds[bed++]={lo,hi,top};
        part({x,(bottom+top)*.5f,z},{.8f,top-bottom,.8f},Role::Footing,.92f);
        part({x,(top+head)*.5f,z},{.34f,head-top,.34f},Role::Pier,.68f);
    }
    if(out.parts.size()!=57||out.additions.size()!=25)throw std::runtime_error("BRIDGE_INCOMPLETE_CONSTRUCTION");
    return out;
}
template<class Scene> void Submit(Scene& scene,const Model& model,Aevrynd::Material3D wood,Aevrynd::Material3D stone){
    wood.useUv=true;
    for(const auto& p:model.parts){
        auto material=p.role==Role::Footing?stone:wood;
        material.tint.r*=p.tone;material.tint.g*=p.tone;material.tint.b*=p.tone;
        if(!scene.MaterialBox(p.center,p.size,material))throw std::runtime_error("BRIDGE_FINITE_PART_SUBMISSION_FAILED");
    }
}
}
