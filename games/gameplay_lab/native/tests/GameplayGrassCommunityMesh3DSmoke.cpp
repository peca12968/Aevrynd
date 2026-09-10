#include "GameplayGrassCommunityMesh3D.h"
#include <bit>
#include <iostream>
#include <iomanip>
#include <numeric>
using namespace Aevrynd;
using namespace GameplayLab;
struct SpacingStats{double mean{},cv{};};
SpacingStats PointSpacing(const std::vector<Vec3>& points){
 std::vector<double> nearest;nearest.reserve(points.size());
 for(std::size_t point=0;point<points.size();++point){
  const double x=points[point].x,z=points[point].z;double best=std::numeric_limits<double>::max();
  for(std::size_t other=0;other<points.size();++other)if(other!=point){const double dx=x-points[other].x,dz=z-points[other].z;best=(std::min)(best,std::sqrt(dx*dx+dz*dz));}
  nearest.push_back(best);
 }
 const double mean=std::accumulate(nearest.begin(),nearest.end(),0.0)/nearest.size();double variance{};
 for(const double value:nearest)variance+=(value-mean)*(value-mean);
 return {mean,std::sqrt(variance/nearest.size())/mean};
}
SpacingStats BladeSpacing(const Mesh3D& mesh,unsigned segments){
 const std::size_t stride=std::size_t((std::clamp)(segments,2u,5u))*2u+2u,count=mesh.vertices.size()/stride;
 std::vector<Vec3> points;points.reserve(count);
 for(std::size_t blade=0;blade<count;++blade){const auto& a=mesh.vertices[blade*stride];const auto& b=mesh.vertices[blade*stride+1];points.push_back({(a.x+b.x)*.5f,0,(a.z+b.z)*.5f});}
 return PointSpacing(points);
}
std::vector<Vec3> BladeCenters(const Mesh3D& mesh,unsigned segments){
 const std::size_t stride=std::size_t((std::clamp)(segments,2u,5u))*2u+2u,count=mesh.vertices.size()/stride;
 std::vector<Vec3> points;points.reserve(count);
 for(std::size_t blade=0;blade<count;++blade){const auto& a=mesh.vertices[blade*stride];const auto& b=mesh.vertices[blade*stride+1];points.push_back({(a.x+b.x)*.5f,0,(a.z+b.z)*.5f});}
 return points;
}
struct IslandStats{unsigned significantComponents{};double maximumSpanFraction{};};
IslandStats ShortIslandStructure(const std::vector<Vec3>& points){
 const auto spacing=PointSpacing(points);const double link=spacing.mean*1.45;
 float minX=points.front().x,maxX=minX,minZ=points.front().z,maxZ=minZ;
 for(const auto point:points){minX=(std::min)(minX,point.x);maxX=(std::max)(maxX,point.x);minZ=(std::min)(minZ,point.z);maxZ=(std::max)(maxZ,point.z);}
 const double fullSpan=std::hypot(double(maxX-minX),double(maxZ-minZ));
 std::vector<bool> seen(points.size());IslandStats result{};
 for(std::size_t seed=0;seed<points.size();++seed)if(!seen[seed]){
  std::vector<std::size_t> pending{seed},component;seen[seed]=true;
  while(!pending.empty()){const auto item=pending.back();pending.pop_back();component.push_back(item);for(std::size_t other=0;other<points.size();++other)if(!seen[other]){const double dx=points[item].x-points[other].x,dz=points[item].z-points[other].z;if(std::hypot(dx,dz)<=link){seen[other]=true;pending.push_back(other);}}}
  if(component.size()<3)continue;++result.significantComponents;
  double span{};for(const auto a:component)for(const auto b:component)span=(std::max)(span,std::hypot(double(points[a].x-points[b].x),double(points[a].z-points[b].z)));
  result.maximumSpanFraction=(std::max)(result.maximumSpanFraction,span/fullSpan);
 }
 return result;
}
bool NaturalShortIslands(IslandStats value){return value.significantComponents>=3&&value.maximumSpanFraction<.62;}
bool NaturalSpacing(SpacingStats value){return std::isfinite(value.mean)&&std::isfinite(value.cv)&&value.mean>0&&value.cv>=.32;}
Mesh3D Legacy(unsigned blades,unsigned segments){
    const auto tuft=CreateGrassClumpMesh3D(blades,segments);
    constexpr std::array<Vec3,9> offsets{{
      {-.08f,0,.04f},{-1.12f,0,-.24f},{.86f,0,.34f},{-.34f,0,1.18f},
      {.28f,0,-1.26f},{1.42f,0,-.62f},{-1.38f,0,.72f},{.78f,0,1.36f},{-.72f,0,-1.48f}}};
    constexpr std::array<float,9> rotations{{.18f,1.06f,2.31f,3.54f,4.62f,5.71f,.72f,2.82f,4.08f}};
    Mesh3D result;result.vertices.reserve(tuft.vertices.size()*offsets.size());result.indices.reserve(tuft.indices.size()*offsets.size());
    for(std::size_t patch=0;patch<offsets.size();++patch){
      const auto base=std::uint32_t(result.vertices.size());const float scale=.56f+.027f*float((patch*5)%7);
      const float sine=std::sin(rotations[patch]),cosine=std::cos(rotations[patch]);
      for(auto vertex:tuft.vertices){
        const float localX=vertex.x*scale,localZ=vertex.z*scale;
        vertex.x=localX*cosine-localZ*sine+offsets[patch].x;
        vertex.y*=.78f+.027f*float((patch*3)%6);
        vertex.z=localX*sine+localZ*cosine+offsets[patch].z;
        const auto packed=UnpackNormalSnorm8(vertex.normal);
        vertex.normal=PackNormalSnorm8({packed.x*cosine-packed.z*sine,packed.y,packed.x*sine+packed.z*cosine},packed.w);
        result.vertices.push_back(vertex);
      }
      for(const auto index:tuft.indices)result.indices.push_back(base+index);
    }
    return result;
}
bool Exact(const Mesh3D& a,const Mesh3D& b){
 if(a.indices!=b.indices||a.vertices.size()!=b.vertices.size())return false;
 for(std::size_t i=0;i<a.vertices.size();++i){
 const auto& x=a.vertices[i];const auto& y=b.vertices[i];
 if(std::bit_cast<std::array<std::uint32_t,7>>(x)!=std::bit_cast<std::array<std::uint32_t,7>>(y))return false;
 }return true;
}
// 固定 192² 正交取樣；數值是三角投影的離散代理，不宣稱真實 GPU 像素覆蓋。
constexpr int N=192;
struct Projection { std::array<unsigned char,N*N> pixels{}; float area{},hole{}; Vec2 holePosition{}; };
Projection Project(const Mesh3D& mesh,const MeshBounds3D& reference,float angle){
 Projection result;std::array<float,N*N> distance;distance.fill(1e9f);
 const float loX=reference.center.x-reference.extents.x,loZ=reference.center.z-reference.extents.z;
 const float stepX=2.f*reference.extents.x/float(N),stepZ=2.f*reference.extents.z/float(N);
 const auto point=[&](const Vertex3D& v){return Vec2{(v.x-loX)/stepX,(v.z*std::cos(angle)+v.y*std::sin(angle)-loZ)/stepZ};};
 const auto cross=[](Vec2 a,Vec2 b,Vec2 c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};
 for(std::size_t i=0;i<mesh.indices.size();i+=3){
  const auto a=point(mesh.vertices[mesh.indices[i]]),b=point(mesh.vertices[mesh.indices[i+1]]),c=point(mesh.vertices[mesh.indices[i+2]]);
  const float signedArea=cross(a,b,c);if(std::abs(signedArea)<1e-8f)continue;
  const int x0=std::clamp(int(std::floor((std::min)({a.x,b.x,c.x}))),0,N-1),x1=std::clamp(int(std::ceil((std::max)({a.x,b.x,c.x}))),0,N-1);
  const int z0=std::clamp(int(std::floor((std::min)({a.y,b.y,c.y}))),0,N-1),z1=std::clamp(int(std::ceil((std::max)({a.y,b.y,c.y}))),0,N-1);
  for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x){
   const Vec2 q{float(x)+.5f,float(z)+.5f};
   if(cross(a,b,q)*signedArea>=0&&cross(b,c,q)*signedArea>=0&&cross(c,a,q)*signedArea>=0)result.pixels[std::size_t(z*N+x)]=1;
  }
 }
 unsigned count{};
 for(int z=0;z<N;++z)for(int x=0;x<N;++x){
  count+=result.pixels[std::size_t(z*N+x)];
  for(int other=0;other<N;++other)if(result.pixels[std::size_t(z*N+other)]){
   const float delta=float(x-other)*stepX;
   distance[std::size_t(z*N+x)]=(std::min)(distance[std::size_t(z*N+x)],delta*delta);
  }
 }
 // 中央八成區域量最大未覆蓋圓半徑；使用真正三角覆蓋，非 AABB 或頂點點雲。
 for(int z=N/10;z<N-N/10;++z)for(int x=N/10;x<N-N/10;++x){
  float best=1e9f;
  for(int other=0;other<N;++other){
   const float delta=float(z-other)*stepZ;
   best=(std::min)(best,distance[std::size_t(other*N+x)]+delta*delta);
  }
  if(std::sqrt(best)>result.hole){result.hole=std::sqrt(best);result.holePosition={loX+(float(x)+.5f)*stepX,loZ+(float(z)+.5f)*stepZ};}
 }
 result.area=float(count)*stepX*stepZ;
 return result;
}
bool Density(const Mesh3D& mesh,const Mesh3D& legacy){
 const auto bounds=legacy.Bounds();
 const auto old=Project(legacy,bounds,0),now=Project(mesh,bounds,0);
 std::cout<<"XZ area="<<now.area<<" baseline="<<old.area<<" holeRadius="<<now.hole<<" baselineHole="<<old.hole<<" at="<<now.holePosition.x<<','<<now.holePosition.y<<'\n';
 return now.area>=old.area&&now.hole<=old.hole*.6f;
}
double ProjectedLeafArea(const Mesh3D& mesh){
 double area{};
 for(std::size_t i=0;i<mesh.indices.size();i+=3){
  const auto& a=mesh.vertices[mesh.indices[i]];const auto& b=mesh.vertices[mesh.indices[i+1]];const auto& c=mesh.vertices[mesh.indices[i+2]];
  area+=std::abs((double(b.x)-a.x)*(double(c.z)-a.z)-(double(b.z)-a.z)*(double(c.x)-a.x))*.5;
 }
 // 網格保留原本雙 winding，除二只避免重複計算同一葉面，不改提交拓樸。
 return area*.5;
}
unsigned CoarseMask(const Projection& projection){
 unsigned mask{};
 for(int z=0;z<N;++z)for(int x=0;x<N;++x)if(projection.pixels[std::size_t(z*N+x)])mask|=1u<<((z*4/N)*4+x*4/N);
 return mask;
}
int main(){
 std::cout<<std::setprecision(12);
 bool densityPassed=true;
 std::array<unsigned,2> coverageMasks{};unsigned detailIndex{};
 // 與 GameplayLab 正式近／遠景網格一致，避免測試仍只覆蓋舊的稀疏參數。
 for(const auto detail: {std::pair{15u,4u},std::pair{10u,2u}}){
 const auto a=Legacy(detail.first,detail.second),b=CreateGameplayGrassCommunityMesh3D(detail.first,detail.second);
 const auto oldSpacing=BladeSpacing(a,detail.second),newSpacing=BladeSpacing(b,detail.second);
 const auto islands=ShortIslandStructure(BladeCenters(b,detail.second));
 std::cout<<"NEAREST_SPACING mean="<<newSpacing.mean<<" cv="<<newSpacing.cv<<" baselineMean="<<oldSpacing.mean<<" baselineCv="<<oldSpacing.cv<<'\n';
 std::cout<<"SHORT_ISLANDS components="<<islands.significantComponents<<" maximumSpanFraction="<<islands.maximumSpanFraction<<'\n';
 if(!NaturalShortIslands(islands)){std::cerr<<"LONG_COMMUNITY_BAND=FAIL\n";return 14;}
 if(!NaturalSpacing(newSpacing)){std::cerr<<"PERIODIC_GRASS_SPACING=FAIL\n";return 12;}
 if(!Exact(a,CreateGameplayGrassCommunityBaseline3D(detail.first,detail.second))){std::cerr<<"EXTRACTION_BIT_EXACT=FAIL\n";return 7;}
 std::cout<<"EXTRACTION_BIT_EXACT=PASS\n";
 if(!Exact(b,CreateGameplayGrassCommunityMesh3D(detail.first,detail.second))){std::cerr<<"DETERMINISM=FAIL\n";return 1;}
 std::cout<<"DETERMINISM=PASS blades="<<detail.first<<" vertices="<<b.vertices.size()<<" triangles="<<b.indices.size()/3<<'\n';
 densityPassed=Density(b,a)&&densityPassed;
 const auto oldArea=ProjectedLeafArea(a),newArea=ProjectedLeafArea(b);
 std::cout<<"TOTAL_LEAF_AREA value="<<newArea<<" baseline="<<oldArea<<" delta="<<newArea-oldArea<<'\n';
 if(newArea<oldArea){std::cerr<<"TOTAL_LEAF_AREA_REGRESSION\n";return 11;}
 coverageMasks[detailIndex++]=CoarseMask(Project(b,a.Bounds(),0));
 auto boundsOnly=a;auto unused=a.vertices.front();unused.x+=100.f;unused.z+=100.f;boundsOnly.vertices.push_back(unused);
 if(Density(boundsOnly,a)){std::cerr<<"HOSTILE_BOUNDS_ONLY=FALSE_PASS\n";return 8;}
 std::cout<<"HOSTILE_BOUNDS_ONLY=REJECTED\n";
 const auto ab=a.Bounds(),bb=b.Bounds();
 if(std::bit_cast<std::array<std::uint32_t,7>>(ab)!=std::bit_cast<std::array<std::uint32_t,7>>(bb)){
  std::cerr<<"BOUNDS_BIT_EXACT=FAIL\n";return 3;
 }
 if(a.indices!=b.indices||a.vertices.size()!=b.vertices.size())return 4;
 unsigned roots{},tips{};
 for(std::size_t i=0;i<a.vertices.size();++i){
  const auto& old=a.vertices[i];const auto& now=b.vertices[i];
  if(!std::isfinite(now.x)||!std::isfinite(now.z)||old.y!=now.y||old.normal!=now.normal||old.color!=now.color||old.u!=now.u||old.v!=now.v)return 5;
  const float weight=UnpackNormalSnorm8(now.normal).w;
  roots+=now.y==0.f&&weight>.95f;tips+=weight<.05f;
 }
 if(roots!=detail.first*18u||tips!=detail.first*18u)return 6;
 std::cout<<"BOUNDS_HEIGHT_INDICES_WIND=PASS roots="<<roots<<" tips="<<tips<<'\n';
 for(const float yaw:{0.f,1.570796327f,3.141592654f,4.712388981f}){
  const auto view=[&](Mesh3D mesh){for(auto& vertex:mesh.vertices){const float x=vertex.x,z=vertex.z;vertex.x=x*std::cos(yaw)+z*std::sin(yaw);vertex.z=(z*std::cos(yaw)-x*std::sin(yaw))*.707106781f+vertex.y*.707106781f;}return mesh;};
  const auto oldView=view(a),newView=view(b);const auto reference=oldView.Bounds();
  const auto oldProjection=Project(oldView,reference,0),newProjection=Project(newView,reference,0);
  std::cout<<"OBLIQUE yaw="<<yaw<<" area="<<newProjection.area<<" baseline="<<oldProjection.area<<'\n';
  if(newProjection.area<oldProjection.area){std::cerr<<"SILHOUETTE_REGRESSION\n";return 9;}
 }
 }
 unsigned hostileVariants{};
 for(const unsigned columns:{6u,9u,10u})for(const float yaw:{0.f,.37f})for(const float phase:{.13f,.61f}){
  std::vector<Vec3> lattice;const unsigned rows=90u/columns;const float sine=std::sin(yaw),cosine=std::cos(yaw);
  for(unsigned z=0;z<rows;++z)for(unsigned x=0;x<columns;++x){const float px=(float(x)+phase)*.19f,pz=(float(z)+phase*.43f)*.19f;lattice.push_back({px*cosine-pz*sine,0,px*sine+pz*cosine});}
  if(NaturalSpacing(PointSpacing(lattice))){std::cerr<<"HOSTILE_PERIODIC_LATTICE=FALSE_PASS\n";return 13;}
 ++hostileVariants;
 }
 for(const float yaw:{0.f,.47f,1.13f}){
  std::vector<Vec3> bands;const float sine=std::sin(yaw),cosine=std::cos(yaw);
  for(unsigned band=0;band<3;++band)for(unsigned item=0;item<45;++item){const float u=-1.5f+3.f*float(item)/44.f;const float v=(float(band)-1.f)*.62f+.11f*std::sin(u*2.3f+float(band)*1.7f);bands.push_back({u*cosine-v*sine,0,u*sine+v*cosine});}
  if(NaturalShortIslands(ShortIslandStructure(bands))){std::cerr<<"HOSTILE_LONG_SINE_BANDS=FALSE_PASS\n";return 15;}
 }
 std::cout<<"HOSTILE_LONG_SINE_BANDS=REJECTED variants=3\n";
 std::cout<<"HOSTILE_PERIODIC_LATTICE=REJECTED variants="<<hostileVariants<<'\n';
 std::cout<<"HIGH_LOW_MASKS "<<coverageMasks[0]<<'/'<<coverageMasks[1]<<'\n';
 if(coverageMasks[0]!=0xffffu||coverageMasks[1]!=coverageMasks[0])return 10;
 if(!densityPassed){std::cerr<<"PROJECTED_GRASS_DENSITY=FAIL\n";return 2;}
 std::cout<<"GAMEPLAY_GRASS_COMMUNITY=PASS\n";
 return 0;
}
