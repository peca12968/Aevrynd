// 單一真實投影約束的回歸；不是完整BodySweep、角色步進或整幀資格。
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

namespace ProjectionResidualTrial {
using Aevrynd::BodySweep;
using Aevrynd::Vec3;
unsigned checks{},cases{},actualCases{},unchangedCases{},nonfiniteCases{},boundedCases{};
void Check(bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);}
float F(std::uint32_t word){volatile std::uint32_t input=word;return std::bit_cast<float>(static_cast<std::uint32_t>(input));}
std::uint32_t Word(float value){return std::bit_cast<std::uint32_t>(value);}
bool EqualWords(Vec3 a,Vec3 b){return Word(a.x)==Word(b.x)&&Word(a.y)==Word(b.y)&&Word(a.z)==Word(b.z);}
bool FiniteVector(Vec3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
double DoubleDot(Vec3 a,Vec3 b){return double(a.x)*double(b.x)+double(a.y)*double(b.y)+double(a.z)*double(b.z);}
std::uint32_t Ordered(float value){const auto word=Word(value);return word&0x80000000u?~word:word^0x80000000u;}
std::uint64_t Ulps(float a,float b){const auto x=Ordered(a),y=Ordered(b);return x>y?std::uint64_t(x)-y:std::uint64_t(y)-x;}
Vec3 Normal(){return {F(0x3F4CECBBu),F(0),F(0xBF196EFDu)};}
Vec3 Motion(){return {F(0xBC984EBBu),F(0x3AF5522Fu),F(0xBCCB6BBCu)};}
Vec3 Velocity(){return {F(0xC0E3CF88u),F(0x3F377780u),F(0xC1182192u)};}
BodySweep Constraint(Vec3 normal){
    BodySweep hit;
    // 原log只側錄index0/origin2的一個約束；其餘原生hit狀態沒有證據，不捏造。
    hit.count=1;hit.normals[0]=normal;hit.origins[0]=Aevrynd::DynamicSideContact;
    return hit;
}
bool Apply(const BodySweep& hit,Vec3& motion,Vec3& velocity){
#ifdef HAS_REPAIR
    return Aevrynd::CompleteDynamicProjection(hit,motion,velocity);
#else
    return Aevrynd::DynamicProjectionFeasible(hit,motion,velocity);
#endif
}
bool StrictAll(const BodySweep& hit,Vec3 motion,Vec3 velocity){
    if(!FiniteVector(motion)||!FiniteVector(velocity))return false;
    for(std::size_t i=0;i<hit.count;++i)for(const Vec3 value:{motion,velocity}){
        const float single=Aevrynd::Dot(hit.normals[i],value);const double wide=DoubleDot(hit.normals[i],value);
        if(!std::isfinite(single)||!std::isfinite(wide)||single<0||wide<0)return false;
    }
    return Aevrynd::DynamicProjectionFeasible(hit,motion,velocity);
}
void Words(const char* label,Vec3 value){
    std::cout<<label<<'='<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(8)<<Word(value.x)<<','
        <<std::setw(8)<<Word(value.y)<<','<<std::setw(8)<<Word(value.z)<<std::dec;
}
Vec3 Symmetry(Vec3 value,unsigned kind){
    if(kind==1)std::swap(value.x,value.z);
    else if(kind==2)value.x=-value.x;
    else if(kind==3)value.z=-value.z;
    return value;
}
void Actual(unsigned symmetry){
    const Vec3 normal=Symmetry(Normal(),symmetry),beforeMotion=Symmetry(Motion(),symmetry),beforeVelocity=Symmetry(Velocity(),symmetry);
    const auto hit=Constraint(normal);auto motion=beforeMotion,velocity=beforeVelocity;
    Check(!Aevrynd::DynamicProjectionFeasible(hit,motion,velocity),"ACTUAL_OR_SYMMETRIC_NEGATIVE_RESIDUAL_NOT_REPRODUCED");
    Check(Aevrynd::Dot(normal,motion)==-0x1p-30f,"ACTUAL_FLOAT_MOTION_NOT_MINUS_TWO_POW_MINUS30");
    Check(Aevrynd::Dot(normal,velocity)==0.f&&DoubleDot(normal,velocity)==-0x1.44b88p-26,
        "ACTUAL_FLOAT_ZERO_BUT_DOUBLE_NEGATIVE_VELOCITY_CHANGED");
    Check(DoubleDot(normal,motion)==-0x1.4d2e68p-32,"ACTUAL_DOUBLE_MOTION_BITS_CHANGED");
    std::cout<<"PROJECTION_RESIDUAL_BEFORE symmetry="<<symmetry<<" profile="<<PROJECTION_PROFILE_NAME<<' ';
    Words("normal",normal);std::cout<<' ';Words("motion",motion);std::cout<<' ';Words("velocity",velocity);
    std::cout<<" float_motion="<<std::hexfloat<<Aevrynd::Dot(normal,motion)<<" float_velocity="<<Aevrynd::Dot(normal,velocity)
        <<" double_motion="<<DoubleDot(normal,motion)<<" double_velocity="<<DoubleDot(normal,velocity)<<std::defaultfloat<<'\n'<<std::flush;
    Check(Apply(hit,motion,velocity),"ACTUAL_NEGATIVE_RESIDUAL_MUST_BE_REPAIRED_FOR_SAFE_CONTINUATION");
    Check(StrictAll(hit,motion,velocity),"REPAIRED_VECTOR_NOT_STRICTLY_FEASIBLE_FLOAT_AND_DOUBLE");
    Check(Word(motion.y)==Word(beforeMotion.y)&&Word(velocity.y)==Word(beforeVelocity.y),"SIDE_REPAIR_CHANGED_VERTICAL_MOTION_OR_GRAVITY_STATE");
    Check((motion.x!=0||motion.z!=0)&&(velocity.x!=0||velocity.z!=0),"REPAIR_ZEROED_NONZERO_TANGENT");
    Check(double(motion.x)*beforeMotion.x+double(motion.z)*beforeMotion.z>0&&
        double(velocity.x)*beforeVelocity.x+double(velocity.z)*beforeVelocity.z>0,"REPAIR_REVERSED_TANGENT");
    const std::array<std::uint64_t,6> changed{Ulps(beforeMotion.x,motion.x),Ulps(beforeMotion.y,motion.y),Ulps(beforeMotion.z,motion.z),
        Ulps(beforeVelocity.x,velocity.x),Ulps(beforeVelocity.y,velocity.y),Ulps(beforeVelocity.z,velocity.z)};
    for(auto amount:changed)Check(amount<=16,"SINGLE_CONSTRAINT_REPAIR_EXCEEDED16ULP_COMPONENT_BOUND");
    std::cout<<"PROJECTION_RESIDUAL_AFTER symmetry="<<symmetry<<' ';Words("motion",motion);std::cout<<' ';Words("velocity",velocity);
    std::cout<<" component_ulps=";for(auto amount:changed)std::cout<<amount<<',';
    std::cout<<" strict_float_double=1 original_DP=1 vertical_words_unchanged=1\n";
    ++cases;++actualCases;
}
void AlreadyFeasible(Vec3 motion,Vec3 velocity){
    const auto hit=Constraint(Normal());const auto beforeMotion=motion,beforeVelocity=velocity;
    Check(Aevrynd::DynamicProjectionFeasible(hit,motion,velocity),"ALREADY_FEASIBLE_CONTROL_NOT_TRUE");
    Check(Apply(hit,motion,velocity),"ALREADY_FEASIBLE_CONTROL_REJECTED");
    Check(EqualWords(motion,beforeMotion)&&EqualWords(velocity,beforeVelocity),"ALREADY_FEASIBLE_INPUT_WORDS_CHANGED");
    ++cases;++unchangedCases;
}
void Nonfinite(){
    for(auto word:{0x7FC12345u,0x7F800000u,0xFF800000u})for(unsigned owner=0;owner<2;++owner)for(unsigned axis=0;axis<3;++axis){
        const auto hit=Constraint(Normal());auto motion=Motion(),velocity=Velocity();
        float* slots[3]= {owner?&velocity.x:&motion.x,owner?&velocity.y:&motion.y,owner?&velocity.z:&motion.z};*slots[axis]=F(word);
        const auto beforeMotion=motion,beforeVelocity=velocity;
        Check(!Apply(hit,motion,velocity),"NONFINITE_VECTOR_ACCEPTED");
        Check(EqualWords(motion,beforeMotion)&&EqualWords(velocity,beforeVelocity),"NONFINITE_REJECTION_PARTIALLY_WROTE_VECTORS");
        ++cases;++nonfiniteCases;
    }
}
void BoundedMultiple(){
    auto hit=Constraint({-1,0,0});hit.count=2;hit.normals[1]={1,0,0};hit.origins[1]=Aevrynd::StaticContact;
    Vec3 motion{1,F(0x3AF5522Fu),-.3f},velocity{.5f,F(0x3F377780u),-1.2f};
    const auto beforeMotion=motion,beforeVelocity=velocity;
    // 零X確實可行，但從1/.5出發最多16次向外1ULP到不了0；不是宣稱相反法線無解。
    Check(StrictAll(hit,{0,motion.y,motion.z},{0,velocity.y,velocity.z}),"BOUNDED_CONTROL_ZERO_X_NOT_ACTUALLY_FEASIBLE");
    Check(!Apply(hit,motion,velocity),"BOUNDED_MULTI_CONSTRAINT_UNRESOLVED_INPUT_FALSELY_ACCEPTED");
    Check(EqualWords(motion,beforeMotion)&&EqualWords(velocity,beforeVelocity),"BOUNDED_FAILURE_NOT_ATOMIC");
    ++cases;++boundedCases;
}
}

int RunProjectionResidualCases()try{
    using namespace ProjectionResidualTrial;
    Actual(0);
    for(unsigned symmetry=1;symmetry<4;++symmetry)Actual(symmetry);
    AlreadyFeasible({1,F(0x3AF5522Fu),0},{2,F(0x3F377780u),0});
    AlreadyFeasible({F(0),F(0x80000000u),F(0x80000000u)},{F(0x80000000u),.75f,F(0)});
    Nonfinite();BoundedMultiple();
    Check(cases==25&&actualCases==4&&unchangedCases==2&&nonfiniteCases==18&&boundedCases==1,"EXACT25_CASE_INVENTORY");
    std::cout<<"PROJECTION_RESIDUAL_CPU_PASS cases=25 actual_symmetries=4 feasible_unchanged=2 nonfinite_rejected=18 bounded_multi_rejected=1 checks="
        <<checks<<" profile="<<PROJECTION_PROFILE_NAME<<" full_controller=0 main=0 GPU=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"PROJECTION_RESIDUAL_FIRST_DIVERGENCE="<<error.what()<<'\n';return 1;}
