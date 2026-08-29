#include "GameplayRules3D.h"
#include <Aevrynd/PhysicsWorld3D.h>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
using namespace Aevrynd;
struct Receipt { std::array<Vec3,16> positions{}; PhysicsWorldStats3D stats{}; std::uint64_t ruleTicks{}; float grabbedMotion{}; };
Receipt Simulate() {
  GameplayLab::Rules3D rules; PhysicsWorld3D physics;
  RigidBodySettings3D ground{}; ground.type=RigidBodyType3D::Static; ground.transform.position={0,-.5f,0}; ground.colliderExtents={40,.5f,40}; physics.CreateBody(ground);
  std::array<RigidBodyHandle3D,16> handles{};
  for(int i=0;i<16;++i){RigidBodySettings3D body{};body.type=RigidBodyType3D::Dynamic;body.transform.position={float(i%4)*1.2f,.55f+float(i/4)*1.1f,float((i/4)%2)*1.2f};if(i==15){body.transform.position={18,.5f,18};body.gravityScale=0;}handles[i]=physics.CreateBody(body);}
  float grabbedMotion{};
  for(int i=0;i<10000;++i){GameplayLab::TickInput input{};input.movement={float((i%3)-1),0,float(((i/3)%3)-1)};input.run=(i&1)!=0;input.jump=i%181==0;input.attack=i%37==0;rules.Tick(input);if(i==120)physics.ApplyImpulse(handles[0],{8,12,0});if(i>180&&i<300)physics.AddForce(handles[1],{80,0,0});physics.Step(1.f/60.f,2);if(i<600){grabbedMotion=(std::max)(grabbedMotion,Length(physics.GetBody(handles[0])->settings.transform.position-Vec3{0,.55f,0}));grabbedMotion=(std::max)(grabbedMotion,Length(physics.GetBody(handles[1])->settings.transform.position-Vec3{1.2f,.55f,0}));}}
  Receipt result{};for(std::size_t i=0;i<handles.size();++i)result.positions[i]=physics.GetBody(handles[i])->settings.transform.position;result.stats=physics.Stats();result.ruleTicks=rules.GetStats().ticks;result.grabbedMotion=grabbedMotion;return result;
}
int main(){const auto start=std::chrono::steady_clock::now();const Receipt a=Simulate(),b=Simulate();const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();if(ns>2000000000ll||a.ruleTicks!=10000||a.stats.steps!=10000||a.stats.substeps!=20000||a.stats.bodies!=17||!a.stats.contacts||!a.stats.sleepTransitions)return 1;if(a.stats.contacts!=b.stats.contacts||a.stats.sleepTransitions!=b.stats.sleepTransitions||std::abs(a.grabbedMotion-b.grabbedMotion)>1e-5f)return 2;float centerY{},maximumRadius{};for(std::size_t i=0;i<a.positions.size();++i){const Vec3 p=a.positions[i],q=b.positions[i];if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||Length(p-q)>1e-5f||p.y<.49f||std::abs(p.x)>40||std::abs(p.z)>40)return 3;centerY+=p.y;maximumRadius=(std::max)(maximumRadius,Length(p));}centerY/=float(a.positions.size());if(centerY<.5f||centerY>8.f||maximumRadius>45.f)return 4;if(a.grabbedMotion<.25f)return 5;std::cout<<"runs=2 ticks=10000 totalNs="<<ns<<" contacts="<<a.stats.contacts<<" sleeps="<<a.stats.sleepTransitions<<" motion="<<a.grabbedMotion<<" centerY="<<centerY<<"\n";return 0;}
