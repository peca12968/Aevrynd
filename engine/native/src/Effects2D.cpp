#include <Aevrynd/Effects2D.h>
#include <algorithm>
#include <cmath>
#include <iterator>

namespace Aevrynd {
namespace {
constexpr float Tau=6.283185307f;
uint32_t NextRandom(uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
float UnitRandom(uint32_t& state){return float(NextRandom(state)&0x00ffffffu)/float(0x01000000u);}
float Mix(float start,float end,float amount){return start+(end-start)*amount;}
}

ParticleSystem2D::ParticleSystem2D(std::size_t capacity,std::size_t retainedCapacity):capacity_((std::max)(std::size_t{1},capacity)),retainedCapacity_((std::min)(capacity_,retainedCapacity)){particles_.reserve(retainedCapacity_);}

void ParticleSystem2D::Clear(bool resetStats){particles_.clear();if(resetStats)stats_={};}

void ParticleSystem2D::Emit(Vec2 origin,Vec2 velocity,Color color,float life,float size,const ParticleBurst2D& settings){if(particles_.size()>=capacity_){++stats_.dropped;return;}if(particles_.size()==particles_.capacity()){const std::size_t current=particles_.capacity(),doubled=current&&current<=capacity_/2?current*2:capacity_;particles_.reserve((std::min)(capacity_,(std::max)(particles_.size()+1,(std::max)(std::size_t{1},doubled))));}const float safeLife=(std::max)(.001f,life),safeSize=(std::max)(0.f,size);particles_.push_back({origin,velocity,color,safeLife,safeLife,safeSize,safeSize*(std::max)(0.f,settings.endSizeScale),settings.gravity,(std::max)(0.f,settings.drag),settings.shape});++stats_.emitted;stats_.peakActive=(std::max)(stats_.peakActive,particles_.size());}

ReusableMemoryStats ParticleSystem2D::MemoryStats()const{const std::size_t bytes=particles_.capacity()*sizeof(Particle),target=(std::max)(particles_.size(),retainedCapacity_),reclaimable=particles_.capacity()>target?(particles_.capacity()-target)*sizeof(Particle):0;return{bytes,reclaimable,lastReleasedBytes_,trimCount_};}

std::size_t ParticleSystem2D::TrimUnusedMemory(){const std::size_t before=MemoryStats().retainedCapacityBytes;std::vector<Particle> replacement;replacement.reserve((std::max)(particles_.size(),retainedCapacity_));replacement.insert(replacement.end(),std::make_move_iterator(particles_.begin()),std::make_move_iterator(particles_.end()));particles_.swap(replacement);const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_;}

void ParticleSystem2D::EmitBurst(Vec2 origin,Color color,std::size_t count,ParticleBurst2D settings,uint32_t seed){uint32_t state=seed?seed:0x9e3779b9u;const float minimumSpeed=(std::max)(0.f,(std::min)(settings.minimumSpeed,settings.maximumSpeed)),maximumSpeed=(std::max)(minimumSpeed,(std::max)(settings.minimumSpeed,settings.maximumSpeed));const float minimumLife=(std::max)(.001f,(std::min)(settings.minimumLife,settings.maximumLife)),maximumLife=(std::max)(minimumLife,(std::max)(settings.minimumLife,settings.maximumLife));const float minimumSize=(std::max)(0.f,(std::min)(settings.minimumSize,settings.maximumSize)),maximumSize=(std::max)(minimumSize,(std::max)(settings.minimumSize,settings.maximumSize));for(std::size_t index=0;index<count;++index){const float angle=UnitRandom(state)*Tau,speed=Mix(minimumSpeed,maximumSpeed,UnitRandom(state)),life=Mix(minimumLife,maximumLife,UnitRandom(state)),size=Mix(minimumSize,maximumSize,UnitRandom(state));Emit(origin,{std::cos(angle)*speed+settings.velocityBias.x,std::sin(angle)*speed+settings.velocityBias.y},color,life,size,settings);}}

void ParticleSystem2D::EmitRing(Vec2 origin,Color color,std::size_t count,float speed,ParticleBurst2D settings,float phase){if(!count)return;settings.minimumSpeed=settings.maximumSpeed=(std::max)(0.f,speed);const float life=(std::max)(.001f,(settings.minimumLife+settings.maximumLife)*.5f),size=(std::max)(0.f,(settings.minimumSize+settings.maximumSize)*.5f);for(std::size_t index=0;index<count;++index){const float angle=phase+Tau*float(index)/float(count);Emit(origin,{std::cos(angle)*speed+settings.velocityBias.x,std::sin(angle)*speed+settings.velocityBias.y},color,life,size,settings);}}

void ParticleSystem2D::Update(float deltaTime){const float dt=Clamp(deltaTime,0.f,.1f);for(std::size_t index=0;index<particles_.size();){Particle& particle=particles_[index];particle.remaining-=dt;if(particle.remaining<=0.f){particle=particles_.back();particles_.pop_back();++stats_.recycled;continue;}const float damping=(std::max)(0.f,1.f-particle.drag*dt);particle.velocity=particle.velocity*damping+Vec2{0,particle.gravity*dt};particle.position=particle.position+particle.velocity*dt;++index;}}

void ParticleSystem2D::Render(DrawList& draw)const{RenderInternal(draw,nullptr);}

void ParticleSystem2D::Render(DrawList& draw,Bounds2D worldBounds)const{RenderInternal(draw,&worldBounds);}

void ParticleSystem2D::RenderInternal(DrawList& draw,const Bounds2D* worldBounds)const{stats_.rendered=stats_.visibilityCulled=0;++stats_.renderCalls;Bounds2D bounds{};bool cull=false;if(worldBounds&&std::isfinite(worldBounds->minimum.x)&&std::isfinite(worldBounds->minimum.y)&&std::isfinite(worldBounds->maximum.x)&&std::isfinite(worldBounds->maximum.y)){bounds.minimum={(std::min)(worldBounds->minimum.x,worldBounds->maximum.x),(std::min)(worldBounds->minimum.y,worldBounds->maximum.y)};bounds.maximum={(std::max)(worldBounds->minimum.x,worldBounds->maximum.x),(std::max)(worldBounds->minimum.y,worldBounds->maximum.y)};cull=true;++stats_.visibilityRenderCalls;}for(const Particle& particle:particles_){const float life=Clamp(particle.remaining/particle.lifetime,0.f,1.f),size=Mix(particle.endSize,particle.startSize,life),radius=size*2.5f;if(cull&&(!std::isfinite(particle.position.x)||!std::isfinite(particle.position.y)||particle.position.x+radius<bounds.minimum.x||particle.position.x-radius>bounds.maximum.x||particle.position.y+radius<bounds.minimum.y||particle.position.y-radius>bounds.maximum.y)){++stats_.visibilityCulled;continue;}const Color color{particle.color.r,particle.color.g,particle.color.b,particle.color.a*life};if(particle.shape==ParticleShape2D::Circle)draw.Circle(particle.position,size,color,8);else if(particle.shape==ParticleShape2D::Diamond)draw.Diamond(particle.position,size,size*.7f,color);else{const Vec2 direction=Normalize(particle.velocity);draw.Line(particle.position-direction*size*2.5f,particle.position+direction*size,(std::max)(1.f,size*.55f),color);}++stats_.rendered;}}

void ScreenShake2D::AddImpulse(float strength,float duration){if(strength<=0.f||duration<=0.f)return;strength_=(std::max)(strength_,strength);remaining_=(std::max)(remaining_,duration);duration_=(std::max)(duration_,duration);}
void ScreenShake2D::Update(float deltaTime){const float dt=(std::max)(0.f,deltaTime);remaining_=(std::max)(0.f,remaining_-dt);phase_+=dt;if(remaining_<=0.f)strength_=duration_=0.f;}
float ScreenShake2D::Amplitude()const{return duration_>0.f?strength_*Clamp(remaining_/duration_,0.f,1.f):0.f;}
Vec2 ScreenShake2D::Offset()const{const float amplitude=Amplitude();return{std::sin(phase_*71.f)*amplitude,std::cos(phase_*53.f)*amplitude*.65f};}
}
