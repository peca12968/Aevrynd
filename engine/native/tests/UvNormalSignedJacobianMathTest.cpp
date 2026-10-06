// 獨立數學 fixture；尚未編譯／執行，不取代真 HLSL／GPU 驗證。
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <initializer_list>
#include <limits>

namespace {
struct V2 { float x{},y{}; };
struct V3 { float x{},y{},z{}; };
V3 operator+(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V3 operator-(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V3 operator-(V3 a){return {-a.x,-a.y,-a.z};}
V3 operator*(V3 a,float b){return {a.x*b,a.y*b,a.z*b};}
V3 operator/(V3 a,float b){return {a.x/b,a.y/b,a.z/b};}
float Dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V3 Cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V3 Normalize(V3 a){return a*(1.f/std::sqrt(Dot(a,a)));}
bool Finite(V3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
bool Close(V3 a,V3 b){return Dot(a-b,a-b)<4e-10f;}
bool SameBits(V3 a,V3 b){return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z);}
std::uint64_t checks{},negativeOracles{},positiveBitOracles{},fallbackOracles{},providedTangentOracles{};
void Require(bool condition,const char* message){++checks;if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
struct Input {V3 px,py,geometric,sampled;V2 ux,uy;};
bool ExistingGuards(const Input& input){
    const V3 t=input.px*input.uy.y-input.py*input.ux.y;
    const V3 b=-input.px*input.uy.x+input.py*input.ux.x;
    const float tl=Dot(t,t),bl=Dot(b,b),area=Dot(Cross(t,b),Cross(t,b));
    return tl>1e-10f&&tl<1e20f&&bl>1e-10f&&bl<1e20f&&area>1e-16f&&area<1e30f;
}
V3 EmulateUvNormal(const Input& input,bool determinantFix){
    const V3 rawT=input.px*input.uy.y-input.py*input.ux.y;
    const V3 rawB=-input.px*input.uy.x+input.py*input.ux.x;
    if(!ExistingGuards(input))return input.geometric;
    const float determinant=input.ux.x*input.uy.y-input.ux.y*input.uy.x;
    if(determinantFix&&(!std::isfinite(determinant)||determinant==0.f))return input.geometric;
    V3 t=rawT*(1.f/std::sqrt(Dot(rawT,rawT)));
    V3 b=rawB*(1.f/std::sqrt(Dot(rawB,rawB)));
    if(determinantFix&&determinant<0.f){t=-t;b=-b;}
    const V3 mapped=t*input.sampled.x+b*input.sampled.y+input.geometric*input.sampled.z;
    const float length=Dot(mapped,mapped);
    if(!(length>1e-10f&&length<1e20f))return input.geometric;
    return mapped*(1.f/std::sqrt(length));
}
V3 InverseJacobianReference(const Input& input){
    if(!ExistingGuards(input))return input.geometric;
    const float determinant=input.ux.x*input.uy.y-input.ux.y*input.uy.x;
    if(!std::isfinite(determinant)||determinant==0.f)return input.geometric;
    // 真逆 Jacobian，不直接套用候選的符號修正式當 oracle。
    const V3 dpdu=(input.px*input.uy.y-input.py*input.ux.y)/determinant;
    const V3 dpdv=(-input.px*input.uy.x+input.py*input.ux.x)/determinant;
    const V3 mapped=Normalize(dpdu)*input.sampled.x+Normalize(dpdv)*input.sampled.y+input.geometric*input.sampled.z;
    return Normalize(mapped);
}
V3 ProvidedTangentBranch(V3 geometric,V3 tangent,float handedness,V3 sampled,V3 derivativeFallback){
    const float length=Dot(tangent,tangent);
    if(!(length>1e-10f&&length<1e20f&&std::abs(handedness)>.5f&&std::abs(handedness)<1.5f))return derivativeFallback;
    tangent=tangent*(1.f/std::sqrt(length));tangent=tangent-geometric*Dot(geometric,tangent);
    const float projectedLength=Dot(tangent,tangent);
    if(!(projectedLength>1e-10f&&projectedLength<1e20f))return derivativeFallback;
    tangent=tangent*(1.f/std::sqrt(projectedLength));
    const V3 bitangent=Cross(geometric,tangent)*(handedness<0.f?-1.f:1.f);
    const V3 mapped=tangent*sampled.x+bitangent*sampled.y+geometric*sampled.z;
    const float mappedLength=Dot(mapped,mapped);
    return mappedLength>1e-10f&&mappedLength<1e20f?mapped*(1.f/std::sqrt(mappedLength)):geometric;
}
V3 RotateWorld(V3 vector,float angle){
    const float c=std::cos(angle),s=std::sin(angle);
    const V3 x{vector.x,c*vector.y-s*vector.z,s*vector.y+c*vector.z};
    const V3 y{c*x.x+s*x.z,x.y,-s*x.x+c*x.z};
    return {c*y.x-s*y.y,s*y.x+c*y.y,y.z};
}
void MatrixSweep(){
    constexpr std::array<std::array<float,4>,6> uvMaps{{{1.f,0.f,0.f,1.f},{-1.f,0.f,0.f,1.f},{1.f,0.f,0.f,-1.f},{-1.f,0.f,0.f,-1.f},{.8660254f,-.5f,.5f,.8660254f},{1.f,.35f,0.f,1.f}}};
    const std::array<V3,3> samples{{{.4f,.2f,std::sqrt(.8f)},{-.3f,.5f,std::sqrt(.66f)},{0.f,0.f,1.f}}};
    for(int world=0;world<8;++world){
        const float worldAngle=static_cast<float>(world)*.27f;
        const V3 authoredT=RotateWorld({1.f,0.f,0.f},worldAngle),authoredB=RotateWorld({0.f,1.f,0.f},worldAngle);
        const V3 geometric=Normalize(Cross(authoredT,authoredB));
        for(int camera=0;camera<24;++camera){
            const float angle=static_cast<float>(camera)*.2617993878f,c=std::cos(angle),s=std::sin(angle);
            for(float screenSign:{-1.f,1.f})for(const auto& uv:uvMaps)for(const V3 sampled:samples){
                // screen Jacobian 的旋轉、縮放、正負方向均獨立於作者 UV map。
                const float j00=c*.75f,j10=s*.75f,j01=-s*screenSign*1.25f,j11=c*screenSign*1.25f;
                const Input input{authoredT*j00+authoredB*j10,authoredT*j01+authoredB*j11,geometric,sampled,{uv[0]*j00+uv[1]*j10,uv[2]*j00+uv[3]*j10},{uv[0]*j01+uv[1]*j11,uv[2]*j01+uv[3]*j11}};
                const V3 legacy=EmulateUvNormal(input,false),candidate=EmulateUvNormal(input,true),reference=InverseJacobianReference(input);
                const float determinant=input.ux.x*input.uy.y-input.ux.y*input.uy.x;
                Require(ExistingGuards(input),"nondegenerate matrix must reach normal mapping");
                Require(Finite(candidate)&&Close(candidate,reference),"candidate must match full inverse Jacobian");
                if(determinant>0.f){Require(SameBits(candidate,legacy),"positive determinant original float bits unchanged");++positiveBitOracles;}
                else if(sampled.x!=0.f||sampled.y!=0.f){Require(!Close(legacy,reference),"legacy must fail signed negative determinant oracle");++negativeOracles;}
                for(float handedness:{-1.f,1.f}){
                    const V3 before=ProvidedTangentBranch(geometric,authoredT,handedness,sampled,legacy);
                    const V3 after=ProvidedTangentBranch(geometric,authoredT,handedness,sampled,candidate);
                    Require(SameBits(before,after),"valid provided/skinned tangent branch unchanged");++providedTangentOracles;
                }
                Require(SameBits(ProvidedTangentBranch(geometric,{},1.f,sampled,candidate),candidate),"invalid tangent reaches corrected derivative fallback");
            }
        }
    }
}
void DegenerateSweep(){
    const float nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
    const std::array<std::array<V2,2>,7> cases{{
        std::array<V2,2>{{V2{0.f,0.f},V2{0.f,0.f}}},
        std::array<V2,2>{{V2{1.f,0.f},V2{2.f,0.f}}},
        std::array<V2,2>{{V2{1e-8f,0.f},V2{0.f,1e-8f}}},
        std::array<V2,2>{{V2{1.f,0.f},V2{1.f,1e-9f}}},
        std::array<V2,2>{{V2{nan,0.f},V2{0.f,1.f}}},
        std::array<V2,2>{{V2{infinity,0.f},V2{0.f,1.f}}},
        std::array<V2,2>{{V2{1.f,1.f},V2{1.f,1.f}}}
    }};
    for(const auto& uv:cases){
        const Input input{{1.f,0.f,0.f},{0.f,1.f,0.f},{0.f,0.f,1.f},{.4f,.2f,std::sqrt(.8f)},uv[0],uv[1]};
        const V3 candidate=EmulateUvNormal(input,true);
        Require(Finite(candidate)&&SameBits(candidate,input.geometric),"zero/near degenerate/nonfinite must retain exact finite geometry normal");++fallbackOracles;
    }
    for(float scale:{1e-4f,1e-3f,.01f})for(float sign:{-1.f,1.f}){
        const Input input{{1.f,0.f,0.f},{0.f,1.f,0.f},{0.f,0.f,1.f},{.4f,.2f,std::sqrt(.8f)},{scale*sign,0.f},{0.f,scale}};
        const V3 candidate=EmulateUvNormal(input,true);
        Require(Finite(candidate)&&Close(candidate,InverseJacobianReference(input)),"near-boundary UV keeps original guards and finite inverse semantics");
    }
}
}
int main(){
    MatrixSweep();DegenerateSweep();
    Require(negativeOracles>0&&positiveBitOracles>0&&fallbackOracles==7&&providedTangentOracles>0,"all required positive/negative/fallback/provided-tangent oracles executed");
    std::cout<<"UV_NORMAL_MATH_PASS checks="<<checks<<" legacy_negative_failures="<<negativeOracles<<" positive_bit_identical="<<positiveBitOracles<<" degenerate_fallbacks="<<fallbackOracles<<" provided_tangent_identical="<<providedTangentOracles<<'\n';
    return 0;
}
