#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
using uint=std::uint32_t;
struct float3 {float x,y,z; float3(float a=0):x(a),y(a),z(a){} float3(float a,float b,float c):x(a),y(b),z(c){} };
struct int3 {int x,y,z; int3(int a,int b,int c):x(a),y(b),z(c){} explicit int3(float3 a):x(int(a.x)),y(int(a.y)),z(int(a.z)){} };
int3 operator+(int3 a,int3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
#define OP(symbol) float3 operator symbol(float3 a,float3 b){return {a.x symbol b.x,a.y symbol b.y,a.z symbol b.z};}
OP(+) OP(-) OP(*) OP(/)
#undef OP
float3 floor(float3 a){return {std::floor(a.x),std::floor(a.y),std::floor(a.z)};}
float3 frac(float3 a){return a-floor(a);}
uint asuint(int a){return std::bit_cast<uint>(a);}
float lerp(float a,float b,float t){return a+(b-a)*t;}
template<class A,class B> float max(A a,B b){return std::max(float(a),float(b));}
template<class A,class B,class C> float clamp(A a,B b,C c){return std::clamp(float(a),float(b),float(c));}
float saturate(float a){return clamp(a,0,1);}
float smoothstep(float a,float b,float x){float t=saturate((x-a)/(b-a));return t*t*(3-2*t);}
float timeSeconds=46800,cloudWindSpeed=12,cloudBaseNoiseRotation=.37f,cloudDetailNoiseRotation=-1.11f;
float cloudBaseAltitude=1200,cloudThickness=1200,cloudCoverage=.54f,cloudDensity=.82f,cloudErosion=.34f;
uint cloudBaseNoiseSeed=0x9e3779b9u,cloudDetailNoiseSeed=0x243f6a88u;
#include "CloudDensity.inc"
float dot(float3 a,float3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float3 norm(float3 a){return a/std::sqrt(dot(a,a));}
float3 cross(float3 a,float3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
int main(){
 // 固定來自第三視角 compare-v1 的相機原始位元，時間取正式場景 preset。
 const std::array<std::array<uint,11>,2> cameras{{{3245972514u,1069294002u,1084514799u,3245122398u,1065979378u,1090846242u,0,1065353216u,0,0,1065749138u},{3246458228u,1068826496u,1085433738u,3245131662u,1065736098u,1090861150u,0,1065353216u,0,0,1065749138u}}};
 bool pass=true;
 for(int phase=0;phase<2;++phase){timeSeconds=phase?68400.f:46800.f;auto c=cameras[phase];auto f=[&](int i){return std::bit_cast<float>(c[i]);};float3 origin{f(0),f(1),f(2)},forward=norm(float3{f(3),f(4),f(5)}-origin),right=norm(cross({0,1,0},forward)),up=cross(forward,right);
 constexpr int W=192,H=108;std::vector<float> depths;std::vector<int> top(W,H),bottom(W,-1);int maximumRun=0,covered=0;float sum=0,maximum=0;
 std::ofstream image(phase?"dusk-density.pgm":"day-density.pgm");image<<"P2\n"<<W<<' '<<H<<"\n255\n";
 for(int y=0;y<H;++y){int run=0;for(int x=0;x<W;++x){float3 ray=norm(forward+right*((2*(x+.5f)/W-1)*std::tan(f(10)*.5f)*16/9)+up*((1-2*(y+.5f)/H)*std::tan(f(10)*.5f)));float tau=0;
 if(ray.y>.004f){float start=max(0,(1200-origin.y)/ray.y),end=std::min(12000.f,(2400-origin.y)/ray.y);if(end>start){float step=(end-start)/16;for(int i=0;i<16;++i)tau+=CloudField(origin+ray*(start+step*(i+.5f)))*smoothstep(.004f,.032f,ray.y)*smoothstep(0,144,12000-start)*.00135f*step;}}
 float alpha=1-std::exp(-tau);image<<int(alpha*255)<<' ';depths.push_back(tau);sum+=tau;maximum=std::max(maximum,tau);if(alpha>.15f){++covered;++run;maximumRun=std::max(maximumRun,run);top[x]=std::min(top[x],y);bottom[x]=std::max(bottom[x],y);}else run=0;
 }image<<'\n';}
 std::sort(depths.begin(),depths.end());int tmin=H,tmax=0,bmin=H,bmax=0;for(int x=0;x<W;++x)if(bottom[x]>=0){tmin=std::min(tmin,top[x]);tmax=std::max(tmax,top[x]);bmin=std::min(bmin,bottom[x]);bmax=std::max(bmax,bottom[x]);}
 std::cout<<(phase?"dusk":"day")<<" horizontalSpan="<<float(maximumRun)/W<<" topRange="<<tmax-tmin<<" bottomRange="<<bmax-bmin<<" coverage="<<float(covered)/(W*H)<<" tauMean="<<sum/(W*H)<<" tauP95="<<depths[depths.size()*95/100]<<" tauMax="<<maximum<<'\n';
 // 此切片明確將超過視域八成的連續密度列視為水平帶；不是既有 GPU gate。
 const float oldCoverage=phase?.189718f:.164159f,oldMean=phase?.284957f:.2528f;
 pass &= maximumRun<=float(W)*.65f && float(covered)/(W*H)>=oldCoverage*.95f && sum/(W*H)>=oldMean*.95f;
 pass &= (tmax-tmin)>=(phase?21.f:11.f)*1.25f && (bmax-bmin)>=(phase?13.f:11.f)*1.25f;
 }
 return pass?0:1;
}
