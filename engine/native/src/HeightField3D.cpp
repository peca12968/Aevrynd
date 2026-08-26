#include <Aevrynd/HeightField3D.h>
#include <algorithm>
#include <cmath>

namespace Aevrynd {
namespace {
uint32_t Hash(int x,int z,uint32_t seed){uint32_t value=seed^uint32_t(x)*0x9e3779b9u^uint32_t(z)*0x85ebca6bu;value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;return value;}
float HashUnit(int x,int z,uint32_t seed){return float(Hash(x,z,seed)&0x00ffffffu)/float(0x01000000u);}
float Smooth(float value){return value*value*(3.f-2.f*value);}
float Lerp(float from,float to,float amount){return from+(to-from)*amount;}
float ValueNoise(float x,float z,uint32_t seed){const int lowX=int(std::floor(x)),lowZ=int(std::floor(z));const float amountX=Smooth(x-float(lowX)),amountZ=Smooth(z-float(lowZ));return Lerp(Lerp(HashUnit(lowX,lowZ,seed),HashUnit(lowX+1,lowZ,seed),amountX),Lerp(HashUnit(lowX,lowZ+1,seed),HashUnit(lowX+1,lowZ+1,seed),amountX),amountZ);}
Color LerpColor(Color from,Color to,float amount){return{Lerp(from.r,to.r,amount),Lerp(from.g,to.g,amount),Lerp(from.b,to.b,amount),Lerp(from.a,to.a,amount)};}
}

void HeightField3D::Configure(HeightFieldSettings3D settings){settings.width=(std::max)(2u,settings.width);settings.depth=(std::max)(2u,settings.depth);settings.cellSize=(std::max)(.05f,settings.cellSize);settings_=settings;heights_.assign(std::size_t(settings.width)*settings.depth,settings.origin.y);minimumHeight_=maximumHeight_=settings.origin.y;}
void HeightField3D::Fill(float height){std::fill(heights_.begin(),heights_.end(),height);minimumHeight_=maximumHeight_=height;}
bool HeightField3D::SetHeight(unsigned x,unsigned z,float height){if(x>=settings_.width||z>=settings_.depth)return false;heights_[Index(x,z)]=height;minimumHeight_=(std::min)(minimumHeight_,height);maximumHeight_=(std::max)(maximumHeight_,height);return true;}
float HeightField3D::Height(unsigned x,unsigned z)const{x=(std::min)(x,settings_.width-1);z=(std::min)(z,settings_.depth-1);return heights_[Index(x,z)];}
Vec3 HeightField3D::SamplePosition(unsigned x,unsigned z)const{return{settings_.origin.x+float(x)*settings_.cellSize,Height(x,z),settings_.origin.z+float(z)*settings_.cellSize};}
bool HeightField3D::Contains(float x,float z)const{return x>=settings_.origin.x&&z>=settings_.origin.z&&x<=settings_.origin.x+float(settings_.width-1)*settings_.cellSize&&z<=settings_.origin.z+float(settings_.depth-1)*settings_.cellSize;}
float HeightField3D::SampleHeight(float x,float z)const{const float localX=Clamp((x-settings_.origin.x)/settings_.cellSize,0.f,float(settings_.width-1)),localZ=Clamp((z-settings_.origin.z)/settings_.cellSize,0.f,float(settings_.depth-1));const unsigned x0=unsigned(std::floor(localX)),z0=unsigned(std::floor(localZ)),x1=(std::min)(x0+1,settings_.width-1),z1=(std::min)(z0+1,settings_.depth-1);const float amountX=localX-float(x0),amountZ=localZ-float(z0);return Lerp(Lerp(Height(x0,z0),Height(x1,z0),amountX),Lerp(Height(x0,z1),Height(x1,z1),amountX),amountZ);}
Vec3 HeightField3D::SampleNormal(float x,float z)const{const float step=settings_.cellSize,left=SampleHeight(x-step,z),right=SampleHeight(x+step,z),back=SampleHeight(x,z-step),front=SampleHeight(x,z+step);return Normalize({left-right,step*2.f,back-front});}
void HeightField3D::RefreshRange(){if(heights_.empty()){minimumHeight_=maximumHeight_=0;return;}const auto [minimum,maximum]=std::minmax_element(heights_.begin(),heights_.end());minimumHeight_=*minimum;maximumHeight_=*maximum;}

void HeightField3D::Generate(uint32_t seed,float amplitude,float frequency,unsigned octaves){amplitude=(std::max)(0.f,amplitude);frequency=(std::max)(.00001f,frequency);octaves=(std::max)(1u,octaves);for(unsigned z=0;z<settings_.depth;++z)for(unsigned x=0;x<settings_.width;++x){const Vec3 position=SamplePosition(x,z);float value{},weight=1.f,totalWeight{},scale=frequency;for(unsigned octave=0;octave<octaves;++octave){value+=(ValueNoise(position.x*scale,position.z*scale,seed+octave*0x9e3779b9u)*2.f-1.f)*weight;totalWeight+=weight;weight*=.5f;scale*=2.f;}heights_[Index(x,z)]=settings_.origin.y+value/totalWeight*amplitude;}RefreshRange();}

void HeightField3D::FlattenCircle(Vec3 center,float innerRadius,float targetHeight,float blendDistance){innerRadius=(std::max)(0.f,innerRadius);blendDistance=(std::max)(0.f,blendDistance);const float outer=innerRadius+blendDistance;for(unsigned z=0;z<settings_.depth;++z)for(unsigned x=0;x<settings_.width;++x){const Vec3 position=SamplePosition(x,z);const float distance=Length(Vec2{position.x-center.x,position.z-center.z});if(distance>outer)continue;const float amount=distance<=innerRadius||blendDistance<=.0001f?1.f:1.f-Smooth((distance-innerRadius)/blendDistance);heights_[Index(x,z)]=Lerp(heights_[Index(x,z)],targetHeight,amount);}RefreshRange();}

Mesh3D HeightField3D::CreateMesh(HeightFieldMeshSettings3D settings)const{Mesh3D mesh;mesh.vertices.reserve(heights_.size());mesh.indices.reserve(std::size_t(settings_.width-1)*(settings_.depth-1)*6);const float range=(std::max)(.0001f,maximumHeight_-minimumHeight_);for(unsigned z=0;z<settings_.depth;++z)for(unsigned x=0;x<settings_.width;++x){const Vec3 position=SamplePosition(x,z),normal=SampleNormal(position.x,position.z);Color color=LerpColor(settings.lowColor,settings.highColor,Clamp((position.y-minimumHeight_)/range,0.f,1.f));if(normal.y<settings.steepNormalY)color=LerpColor(color,settings.steepColor,Clamp((settings.steepNormalY-normal.y)/(std::max)(.001f,settings.steepNormalY),0.f,1.f));mesh.vertices.push_back({position.x,position.y,position.z,PackNormalSnorm8(normal),PackColorRgba8(color)});}for(unsigned z=0;z+1<settings_.depth;++z)for(unsigned x=0;x+1<settings_.width;++x){const uint32_t a=z*settings_.width+x,b=a+1,c=a+settings_.width,d=c+1;mesh.indices.insert(mesh.indices.end(),{a,c,b,b,c,d});}return mesh;}
}
