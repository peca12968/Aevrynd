#include "Aevrynd/HeightField3D.h"
#include "Aevrynd/Renderer.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>
#include <set>
using namespace Aevrynd;
#include "RiverProductionExtract.h"
#include "WaterPreset.inc"
Vec2 operator*(Vec2 a,Vec2 b){return{a.x*b.x,a.y*b.y};}
Vec2 operator+(Vec2 a,float b){return{a.x+b,a.y+b};}
Vec2 operator-(float a,Vec2 b){return{a-b.x,a-b.y};}
Vec2 operator*(float a,Vec2 b){return b*a;}
Vec2& operator+=(Vec2&a,float b){a=a+b;return a;}
using uint=std::uint32_t;
struct Int2{
 int x=0,y=0;
 Int2()=default;
 Int2(int xValue,int yValue):x(xValue),y(yValue){}
 explicit Int2(Vec2 value):x(static_cast<int>(value.x)),y(static_cast<int>(value.y)){}
};
Int2 operator+(Int2 a,Int2 b){return{a.x+b.x,a.y+b.y};}
uint asuint(int value){return static_cast<uint>(value);}
float frac(float a){return a-std::floor(a);}
Vec2 frac(Vec2 a){return{frac(a.x),frac(a.y)};}
Vec2 floor(Vec2 a){return{std::floor(a.x),std::floor(a.y)};}
float dot(Vec2 a,Vec2 b){return a.x*b.x+a.y*b.y;}
float lerp(float a,float b,float w){return a+(b-a)*w;}
float smoothstep(float a,float b,float x){float v=Clamp((x-a)/(b-a),0.f,1.f);return v*v*(3-2*v);}
unsigned NoiseSamples=0;
#include "WaterFoamNoise.inc"
float Legacy(Vec2 p){return smoothstep(.34f,.76f,CloudNoise(p*.42f)*.72f+(.5f+.5f*std::sin(p.y*1.35f+p.x*.31f))*.28f);}
float ProductionFoam(Vec2 p,float time,float shore){const float coverage=WaterFoamCoverage3D(p,time);const float presence=smoothstep(.36f,.70f,coverage+shore*.16f);return Clamp(shore*presence,0.f,1.f);}
template<class F>double GridCorrelation(F f){double energy=0,csX=0,snX=0,csZ=0,snZ=0;for(int z=0;z<256;++z)for(int x=0;x<256;++x){Vec2 p{x*.073f-9.5f,z*.073f-9.5f};double dx=(f({p.x+.002f,p.y})-f({p.x-.002f,p.y}))/.004,dz=(f({p.x,p.y+.002f})-f({p.x,p.y-.002f}))/.004,e=dx*dx+dz*dz;energy+=e;csX+=e*std::cos(6.2831853*.42*p.x);snX+=e*std::sin(6.2831853*.42*p.x);csZ+=e*std::cos(6.2831853*.42*p.y);snZ+=e*std::sin(6.2831853*.42*p.y);}return(std::hypot(csX,snX)+std::hypot(csZ,snZ))/energy;}
bool NoiseGate(){auto current=[](Vec2 p){return WaterFoamCoverage3D(p,0.f);};const double oldGrid=GridCorrelation(Legacy),newGrid=GridCorrelation(current);float maxDelta=0;for(int i=-50;i<=50;++i)for(int j=-50;j<=50;++j){Vec2 p{i*.117f,j*.133f};maxDelta=std::max(maxDelta,std::abs(current({p.x-.00001f,p.y})-current({p.x+.00001f,p.y})));}std::cout<<"noise oldGrid="<<oldGrid<<" newGrid="<<newGrid<<" adjacentDelta="<<maxDelta<<" samplesPerCoverage=2\n";return newGrid<oldGrid*.7&&maxDelta<.003f;}
int main(){
 HeightField3D t({{-256,0,-256},129,129,4});
 if(!t.GenerateUltra(0x4a17u,{10.5f,.0105f,.56f,.22f,24.f,.34f,7,3,.68f,.16f}))return 1;
 t.FlattenCircle({-16,0,8},8,0,8);t.FlattenCircle({-12,0,24},7.2f,0,4.5f);CarveShowcaseRiverShore3D(t);
 const auto mesh=CreateRiverHeroCellMesh3D(t);std::set<std::pair<int,int>> covered;
 const auto cross=[](Vec2 a,Vec2 b,Vec2 c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};
 for(size_t i=0;i<mesh.indices.size();i+=3){Vec2 p[3];for(int j=0;j<3;++j){const auto&v=mesh.vertices[mesh.indices[i+j]];p[j]={v.x*30,v.z*272};}const int x0=int(std::ceil(std::min({p[0].x,p[1].x,p[2].x})*4)),x1=int(std::floor(std::max({p[0].x,p[1].x,p[2].x})*4)),z0=int(std::ceil(std::min({p[0].y,p[1].y,p[2].y})*4)),z1=int(std::floor(std::max({p[0].y,p[1].y,p[2].y})*4));for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x){Vec2 q{x*.25f,z*.25f};if(cross(p[0],p[1],q)<=.00001f&&cross(p[1],p[2],q)<=.00001f&&cross(p[2],p[0],q)<=.00001f)covered.insert({x,z});}}
 std::vector<float> depths;for(auto[x,z]:covered){const float d=-.42f-t.SampleHeight(x*.25f,z*.25f);if(d>0)depths.push_back(d);}
 std::sort(depths.begin(),depths.end());std::cout<<"samples="<<depths.size();for(float q:{.01f,.05f,.10f,.25f,.5f,.9f})std::cout<<" p"<<q<<'='<<depths[size_t(q*(depths.size()-1))];
 const auto water=TestWaterPreset();unsigned active=0,deepActive=0;for(float d:depths){active+=d<water.foamEndDepth;deepActive+=d>=.5f&&d<water.foamEndDepth;}
 std::cout<<" foamStart="<<water.foamStartDepth<<" foamEnd="<<water.foamEndDepth<<" active="<<active<<" deepActive="<<deepActive<<'\n';
 if(water.foamEndDepth>.35f||water.foamStartDepth>.06f||deepActive||!active)return 2;
 const float derivative=std::min(.08f,(water.foamEndDepth-water.foamStartDepth)*.25f);
 if(1-smoothstep(water.foamStartDepth-derivative,water.foamEndDepth+derivative,.35f)>.00001f)return 4;
 NoiseSamples=0;const float sample=WaterFoamCoverage3D({1.3f,-4.7f},2.f);if(!std::isfinite(sample)||NoiseSamples!=2)return 5;
 unsigned gaps=0,coveredFoam=0;float monotoneError=0;
 for(int z=-64;z<=64;++z)for(int x=-64;x<=64;++x){const Vec2 p{x*.173f,z*.151f};const float shallow=ProductionFoam(p,1.25f,1.f);gaps+=shallow<.001f;coveredFoam+=shallow>.15f;float previous=shallow;for(float shore:{.8f,.6f,.4f,.2f,0.f}){const float current=ProductionFoam(p,1.25f,shore);monotoneError=std::max(monotoneError,current-previous);previous=current;}}
 std::cout<<"foamGaps="<<gaps<<" foamCovered="<<coveredFoam<<" monotoneError="<<monotoneError<<'\n';
 if(gaps<256||coveredFoam<256||monotoneError>.00001f)return 7;
 // A broken shoreline must not turn into metre-scale, regularly spaced white
 // slabs in the fixed God camera.  Sample multiple deterministic shore-aligned
 // transects at 5 cm spacing and bound the longest strongly covered island.
 float maximumFoamIslandMetres=0.f;
 for(int transect=-12;transect<=12;++transect){
  unsigned run=0;
  for(int sampleIndex=0;sampleIndex<=800;++sampleIndex){
   const float along=-20.f+float(sampleIndex)*.05f;
   const Vec2 point{along+float(transect)*.173f,
                    along*.31f-float(transect)*.127f};
   if(ProductionFoam(point,1.25f,1.f)>.42f)++run;
   else{maximumFoamIslandMetres=std::max(maximumFoamIslandMetres,float(run)*.05f);run=0;}
  }
  maximumFoamIslandMetres=std::max(maximumFoamIslandMetres,float(run)*.05f);
 }
 std::cout<<"maximumFoamIslandMetres="<<maximumFoamIslandMetres<<'\n';
 if(maximumFoamIslandMetres>1.25f)return 8;
 float maximumTemporalCoverageDelta=0.f;
 for(int z=-32;z<=32;++z)for(int x=-32;x<=32;++x){const Vec2 p{x*.137f,z*.149f};const float reference=WaterFoamCoverage3D(p,0.f);for(float time:{.016f,1.25f,17.f,240.f})maximumTemporalCoverageDelta=std::max(maximumTemporalCoverageDelta,std::abs(reference-WaterFoamCoverage3D(p,time)));}
 std::cout<<"maximumTemporalCoverageDelta="<<maximumTemporalCoverageDelta<<'\n';
 if(maximumTemporalCoverageDelta>.000001f)return 9;
 float seamDelta=0;
 for(int layer=0;layer<2;++layer)for(int edge=-16;edge<=16;++edge)for(int along=-8;along<=8;++along){const float a=layer?.384615f:.8f,b=layer?.923077f:.6f,s=layer?15.73f:9.17f,ox=layer?17.3f:0,oy=layer?9.7f:0;const float qx=(edge-ox)/s,qz=(along*.37f-oy)/s,det=a*a+b*b;Vec2 p{(a*qx+b*qz)/det,(-b*qx+a*qz)/det};const Vec2 e{a*.00001f,-b*.00001f};seamDelta=std::max(seamDelta,std::abs(WaterFoamCoverage3D(p+e,0)-WaterFoamCoverage3D(p-e,0)));}
 std::cout<<"latticeBoundaryDelta="<<seamDelta<<'\n';if(seamDelta>.003f)return 6;
 return NoiseGate()?0:3;
}
