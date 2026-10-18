// 補足混合平面與零位移形狀；直接呼叫候選私有 helper，不複製解算器。
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

#ifndef PROJECTION_PROFILE_NAME
#define PROJECTION_PROFILE_NAME "unspecified"
#endif

namespace MixedPlaneProjectionTrial {
using Aevrynd::BodySweep;
using Aevrynd::Vec3;
unsigned checks{},cases{},mixed{},bounded{},zeroRepair{},zeroCompatibility{};
std::string_view currentCase;
void Check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
float F(std::uint32_t word){volatile std::uint32_t input=word;return std::bit_cast<float>(static_cast<std::uint32_t>(input));}
std::uint32_t Word(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(Vec3 a,Vec3 b){return Word(a.x)==Word(b.x)&&Word(a.y)==Word(b.y)&&Word(a.z)==Word(b.z);}
double DoubleDot(Vec3 n,Vec3 value){return double(n.x)*value.x+double(n.y)*value.y+double(n.z)*value.z;}
std::uint32_t Ordered(float value){const auto word=Word(value);return word&0x80000000u?~word:word^0x80000000u;}
std::uint64_t Ulps(float a,float b){const auto x=Ordered(a),y=Ordered(b);return x>y?std::uint64_t(x)-y:std::uint64_t(y)-x;}
Vec3 Symmetry(Vec3 value,unsigned kind){
    if(kind==1)std::swap(value.x,value.z);
    else if(kind==2)value.x=-value.x;
    else if(kind==3)value.z=-value.z;
    return value;
}
Vec3 Negate(Vec3 value){return {-value.x,-value.y,-value.z};}
void Add(BodySweep& hit,Vec3 n,unsigned char origin){Check(hit.Add(n,origin),"REAL_BODYSWEEP_ADD_FAILED");}
bool SameSweep(const BodySweep& a,const BodySweep& b){
    if(a.count!=b.count||std::bit_cast<std::uint64_t>(a.fraction)!=std::bit_cast<std::uint64_t>(b.fraction)||
       a.origins!=b.origins||a.bodies!=b.bodies||a.touchedBodies!=b.touchedBodies||
       a.dynamicSide!=b.dynamicSide||a.staticWalkable!=b.staticWalkable||
       a.dynamicContact.blockedSide!=b.dynamicContact.blockedSide||
       a.dynamicContact.blockedAbove!=b.dynamicContact.blockedAbove||
       a.dynamicContact.blockedBelow!=b.dynamicContact.blockedBelow)return false;
    for(std::size_t i=0;i<a.normals.size();++i)if(!Same(a.normals[i],b.normals[i]))return false;
    return true;
}
bool StrictAll(const BodySweep& hit,Vec3 motion,Vec3 velocity){
    if(!Aevrynd::Finite(motion)||!Aevrynd::Finite(velocity))return false;
    for(std::size_t i=0;i<hit.count;++i)for(const Vec3 value:{motion,velocity}){
        const float single=Aevrynd::Dot(hit.normals[i],value);const double wide=DoubleDot(hit.normals[i],value);
        if(!std::isfinite(single)||!std::isfinite(wide)||single<0||wide<0)return false;
    }
    return Aevrynd::DynamicProjectionFeasible(hit,motion,velocity);
}
void Finish(const char* group,unsigned order,unsigned symmetry){
    ++cases;std::cout<<"MULTIPLANE_CASE group="<<group<<" order="<<order<<" symmetry="<<symmetry<<" passed=1\n";
}

void MixedSuccess(unsigned order,unsigned symmetry){
    currentCase="mixed-success";
    const float k=F(0x3F3504F3u); // 正確捨入的 float sqrt(.5)，不依賴執行期 sqrt。
    const Vec3 n=Symmetry({k,0,k},symmetry),other=Negate(n);
    BodySweep hit;
    if(order==0){Add(hit,n,Aevrynd::DynamicSideContact);Add(hit,other,Aevrynd::StaticContact);}
    else{Add(hit,other,Aevrynd::StaticContact);Add(hit,n,Aevrynd::DynamicSideContact);}
    Check(hit.count==2,"TWO_OPPOSITE_PLANES_NOT_RETAINED");
    const auto beforeHit=hit;
    const Vec3 beforeMotion=Symmetry({F(0x3F7FFFFFu),F(0x3AF5522Fu),-1.f},symmetry);
    const Vec3 beforeVelocity=Symmetry({F(0x3EFFFFFFu),F(0x3F377780u),-.5f},symmetry);
    auto motion=beforeMotion,velocity=beforeVelocity;
    Check(!Aevrynd::DynamicProjectionFeasible(hit,motion,velocity),"MIXED_INITIAL_DP_MUST_BE_FALSE");
    Check(Aevrynd::CompleteDynamicProjection(hit,motion,velocity),"ONE_ULP_SHARED_TANGENT_NOT_COMPLETED");
    Check(StrictAll(hit,motion,velocity),"MIXED_COMMIT_NOT_FEASIBLE_FOR_EVERY_STATIC_AND_DYNAMIC_PLANE");
    for(std::size_t i=0;i<hit.count;++i)for(const Vec3 value:{motion,velocity})
        Check(Aevrynd::Dot(hit.normals[i],value)==0.f&&DoubleDot(hit.normals[i],value)==0.,"MIXED_RESULT_NOT_EXACT_SHARED_TANGENT");
    Check(Ulps(beforeMotion.x,motion.x)==1&&Ulps(beforeVelocity.x,velocity.x)==1,"TIE_X_REPAIR_NOT_EXACTLY_ONE_ULP");
    Check(Word(beforeMotion.y)==Word(motion.y)&&Word(beforeVelocity.y)==Word(velocity.y),"MIXED_CHANGED_VERTICAL_WORDS");
    Check(Word(beforeMotion.z)==Word(motion.z)&&Word(beforeVelocity.z)==Word(velocity.z),"MIXED_CHANGED_NONSELECTED_AXIS");
    Check((motion.x!=0||motion.z!=0)&&(velocity.x!=0||velocity.z!=0),"MIXED_ZEROED_TANGENT");
    Check(double(motion.x)*beforeMotion.x+double(motion.z)*beforeMotion.z>0&&
          double(velocity.x)*beforeVelocity.x+double(velocity.z)*beforeVelocity.z>0,"MIXED_REVERSED_TANGENT");
    Check(SameSweep(hit,beforeHit),"MIXED_MODIFIED_INPUT_PLANES_OR_METADATA");
    ++mixed;Finish("mixed-success",order,symmetry);
}

void BoundedCorner(unsigned order,unsigned symmetry){
    currentCase="bounded-corner";
    const float k=F(0x3F3504F3u);
    const std::array<Vec3,4> normals{Symmetry({k,0,k},symmetry),Symmetry({-k,0,-k},symmetry),
        Symmetry({-k,0,k},symmetry),Symmetry({k,0,-k},symmetry)};
    const std::array<unsigned char,4> origins{Aevrynd::DynamicSideContact,Aevrynd::StaticContact,
        Aevrynd::DynamicSideContact,Aevrynd::StaticContact};
    BodySweep hit;
    for(unsigned i=0;i<4;++i){const unsigned index=order?3-i:i;Add(hit,normals[index],origins[index]);}
    Check(hit.count==4,"FOUR_CORNER_PLANES_NOT_RETAINED");
    const auto beforeHit=hit;
    const Vec3 beforeMotion=Symmetry({F(0x35800000u),F(0x3AF5522Fu),-F(0x35800000u)},symmetry);
    const Vec3 beforeVelocity=Symmetry({F(0x35000000u),F(0x3F377780u),-F(0x35000000u)},symmetry);
    auto motion=beforeMotion,velocity=beforeVelocity;
    Check(!Aevrynd::DynamicProjectionFeasible(hit,motion,velocity),"CORNER_INITIAL_DP_MUST_BE_FALSE");
    // 四個斜面要求 X=Z=0；零向量確實可行，不能把有界失敗稱為數學無解。
    Check(StrictAll(hit,{0,motion.y,0},{0,velocity.y,0}),"CORNER_ZERO_XZ_WITNESS_NOT_FEASIBLE");
    Check(!Aevrynd::CompleteDynamicProjection(hit,motion,velocity),"BOUNDED_CORNER_UNEXPECTEDLY_ACCEPTED");
    Check(Same(motion,beforeMotion)&&Same(velocity,beforeVelocity),"BOUNDED_CORNER_FAILURE_PARTIALLY_WROTE_VECTORS");
    Check(SameSweep(hit,beforeHit),"BOUNDED_CORNER_MODIFIED_PLANES_OR_METADATA");
    ++bounded;Finish("bounded-corner",order,symmetry);
}

BodySweep RecordedConstraint(unsigned symmetry){
    BodySweep hit;Add(hit,Symmetry({F(0x3F4CECBBu),F(0),F(0xBF196EFDu)},symmetry),Aevrynd::DynamicSideContact);
    Check(hit.count==1,"RECORDED_SINGLE_PLANE_NOT_RETAINED");return hit;
}
void ZeroMotionRepair(unsigned symmetry){
    currentCase="zero-motion-repair";
    auto hit=RecordedConstraint(symmetry);const auto beforeHit=hit;
    Vec3 motion{};
    // 原生 log 的 Motion 具有 float-negative 殘差；這裡明確重用為 velocity 以觸發零 motion 分支形狀。
    const Vec3 beforeVelocity=Symmetry({F(0xBC984EBBu),F(0x3AF5522Fu),F(0xBCCB6BBCu)},symmetry);
    const Vec3 beforeMotion=motion;auto velocity=beforeVelocity;
    Check(!Aevrynd::DynamicProjectionFeasible(hit,motion,velocity),"ZERO_MOTION_NEGATIVE_VELOCITY_NOT_REPRODUCED");
    Check(Aevrynd::CompleteDynamicProjection(hit,motion,velocity),"ZERO_MOTION_VELOCITY_REPAIR_REJECTED");
    Check(StrictAll(hit,motion,velocity),"ZERO_MOTION_REPAIR_NOT_STRICT_ALL_PLANES");
    Check(Same(motion,beforeMotion),"ZERO_MOTION_WORDS_CHANGED");
    Check(Word(velocity.y)==Word(beforeVelocity.y),"ZERO_MOTION_REPAIR_CHANGED_VERTICAL_WORDS");
    Check(Ulps(velocity.x,beforeVelocity.x)<=1&&Ulps(velocity.z,beforeVelocity.z)<=1&&
          !Same(velocity,beforeVelocity),"ZERO_MOTION_REPAIR_NOT_ONE_ULP_LOCAL_CHANGE");
    Check(SameSweep(hit,beforeHit),"ZERO_MOTION_REPAIR_MODIFIED_PLANES");
    ++zeroRepair;Finish("zero-motion-repair",0,symmetry);
}
void ZeroMotionCompatibility(unsigned symmetry){
    currentCase="zero-motion-compatibility";
    auto hit=RecordedConstraint(symmetry);const auto beforeHit=hit;
    Vec3 motion{};const Vec3 beforeMotion=motion;
    const Vec3 beforeVelocity=Symmetry({F(0xC0E3CF88u),F(0x3F377780u),F(0xC1182192u)},symmetry);
    auto velocity=beforeVelocity;
    // 錄到的 Velocity 在原 float DP 是0、double為負；原成功快路徑按核可契約保持不動。
    Check(Aevrynd::DynamicProjectionFeasible(hit,motion,velocity),"RECORDED_ZERO_MOTION_LEGACY_SUCCESS_CHANGED");
    Check(DoubleDot(hit.normals[0],velocity)<0,"RECORDED_COMPATIBILITY_DOUBLE_NEGATIVE_LOST");
    Check(Aevrynd::CompleteDynamicProjection(hit,motion,velocity),"RECORDED_COMPATIBILITY_FAST_PATH_REJECTED");
    Check(Same(motion,beforeMotion)&&Same(velocity,beforeVelocity),"LEGACY_DP_TRUE_FAST_PATH_CHANGED_WORDS");
    Check(SameSweep(hit,beforeHit),"COMPATIBILITY_FAST_PATH_MODIFIED_PLANES");
    ++zeroCompatibility;Finish("zero-motion-compatibility",0,symmetry);
}
}

int RunMixedPlaneProjectionCases()try{
    using namespace MixedPlaneProjectionTrial;
    for(unsigned order=0;order<2;++order)for(unsigned symmetry=0;symmetry<4;++symmetry)MixedSuccess(order,symmetry);
    for(unsigned order=0;order<2;++order)for(unsigned symmetry=0;symmetry<4;++symmetry)BoundedCorner(order,symmetry);
    for(unsigned symmetry=0;symmetry<4;++symmetry)ZeroMotionRepair(symmetry);
    for(unsigned symmetry=0;symmetry<4;++symmetry)ZeroMotionCompatibility(symmetry);
    Check(cases==24&&mixed==8&&bounded==8&&zeroRepair==4&&zeroCompatibility==4,"EXACT24_CASE_INVENTORY");
    std::cout<<"MULTIPLANE_PROJECTION_CPU_PASS cases=24 mixed_success=8 bounded_corner_rejected=8 zero_motion_repaired=4 zero_motion_legacy_unchanged=4 checks="
        <<checks<<" profile="<<PROJECTION_PROFILE_NAME<<" full_controller=0 terrain_advance=0 main=0 GPU=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"MULTIPLANE_PROJECTION_FIRST_DIVERGENCE case="<<MixedPlaneProjectionTrial::currentCase<<" reason="<<error.what()<<'\n';return 1;}
