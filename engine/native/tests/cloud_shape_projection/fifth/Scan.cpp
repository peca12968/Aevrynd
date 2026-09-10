#define main OriginalProjectionMain
#include "../CloudShapeProjection.cpp"
#undef main
using std::isfinite;
#include "Candidate.hlsli"
#include <limits>
struct Metrics {float span,coverage,mean,p95,peak;int top,bottom;};
Metrics Project(int phase,bool original,int samples=16){
 const std::array<std::array<uint,11>,2> cameras{{{3245972514u,1069294002u,1084514799u,3245122398u,1065979378u,1090846242u,0,1065353216u,0,0,1065749138u},{3246458228u,1068826496u,1085433738u,3245131662u,1065736098u,1090861150u,0,1065353216u,0,0,1065749138u}}};
 timeSeconds=phase?68400.f:46800.f;auto c=cameras[phase];auto f=[&](int i){return std::bit_cast<float>(c[i]);};float3 origin{f(0),f(1),f(2)},forward=norm(float3{f(3),f(4),f(5)}-origin),right=norm(cross({0,1,0},forward)),up=cross(forward,right);
 constexpr int W=192,H=108;std::vector<float> depths;std::vector<int> top(W,H),bottom(W,-1);int maximumRun=0,covered=0;float sum=0,peak=0;
 for(int y=0;y<H;++y){int run=0;for(int x=0;x<W;++x){float3 ray=norm(forward+right*((2*(x+.5f)/W-1)*std::tan(f(10)*.5f)*16/9)+up*((1-2*(y+.5f)/H)*std::tan(f(10)*.5f)));float tau=0;
 if(ray.y>.004f){float start=max(0,(1200-origin.y)/ray.y),end=std::min(12000.f,(2400-origin.y)/ray.y);if(end>start){float step=(end-start)/samples;for(int i=0;i<samples;++i){float3 p=origin+ray*(start+step*(i+.5f));tau+=(original?CloudField(p):CandidateCloudDensity(p))*smoothstep(.004f,.032f,ray.y)*smoothstep(0,144,12000-start)*.00135f*step;}}}
 float alpha=1-std::exp(-tau);depths.push_back(tau);sum+=tau;peak=std::max(peak,tau);if(alpha>.15f){++covered;++run;maximumRun=std::max(maximumRun,run);top[x]=std::min(top[x],y);bottom[x]=std::max(bottom[x],y);}else run=0;
 }}std::sort(depths.begin(),depths.end());int tmin=H,tmax=0,bmin=H,bmax=0;for(int x=0;x<W;++x)if(bottom[x]>=0){tmin=std::min(tmin,top[x]);tmax=std::max(tmax,top[x]);bmin=std::min(bmin,bottom[x]);bmax=std::max(bmax,bottom[x]);}
 return {float(maximumRun)/W,float(covered)/(W*H),sum/(W*H),depths[depths.size()*95/100],peak,tmax-tmin,bmax-bmin};
}
bool Gate(Metrics m,Metrics old){return m.span<=.65f&&m.coverage>=old.coverage*.95f&&m.mean>=old.mean*.95f&&m.top>=old.top*1.25f&&m.bottom>=old.bottom*1.25f;}
int main(){
 const auto day=Project(0,true),dusk=Project(1,true);int passing=0;bool hostile=true;std::ofstream out("scan.csv");out<<"frequency,threshold,massScale,warp,asymmetry,phase,span,coverage,mean,p95,max,top,bottom,pass\n";
 for(float frequency:{.00018f,.00022f,.00028f,.00036f,.00050f})for(float threshold:{.40f,.44f,.48f,.52f})for(float mass:{1.f,1.5f})for(float warp:{.4f,.8f})for(float asym:{.15f,.30f}){
 candidateWarp=warp;candidateAsymmetry=asym;candidateFrequency=frequency;candidateThreshold=threshold;candidateMassScale=mass;bool pass=true;
 for(int phase=0;phase<2;++phase){auto m=Project(phase,false);bool good=Gate(m,phase?dusk:day);pass&=good;out<<frequency<<','<<threshold<<','<<mass<<','<<warp<<','<<asym<<','<<phase<<','<<m.span<<','<<m.coverage<<','<<m.mean<<','<<m.p95<<','<<m.peak<<','<<m.top<<','<<m.bottom<<','<<good<<'\n';}
 if(pass){bool convergence=Gate(Project(0,false,32),Project(0,true,32))&&Gate(Project(1,false,32),Project(1,true,32));if(convergence)++passing;std::cout<<"PASS16 "<<frequency<<' '<<threshold<<' '<<mass<<' '<<warp<<' '<<asym<<" pass32="<<convergence<<'\n';}
 for(int i=0;i<100;++i){float3 p{float(i*197-9000),float(1000+i*17),float(2000-i*311)};float a=CandidateCloudDensity(p),b=CandidateCloudShadowDensity(p);hostile &=std::isfinite(a)&&a>=0&&a==b;}
 }
 cloudDensity=0;hostile &= CandidateCloudDensity({0,1800,0})==0;cloudDensity=.82f;cloudThickness=0;hostile &= CandidateCloudDensity({0,1800,0})==0;cloudThickness=1200;
 hostile &= CandidateCloudDensity({0,1199,0})==0&&CandidateCloudDensity({0,2401,0})==0;
 const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
 hostile &= CandidateCloudDensity({nan,1800,0})==0&&CandidateCloudDensity({0,inf,0})==0;
 candidateMassScale=-1;hostile &= CandidateCloudDensity({0,1800,0})==0;candidateMassScale=1;
 timeSeconds=nan;hostile &= CandidateCloudDensity({0,1800,0})==0;timeSeconds=46800;
 std::cout<<"candidates=160 passing="<<passing<<" finiteNonnegativeShadowAndBoundary="<<hostile<<'\n';return passing&&hostile?0:1;
}
