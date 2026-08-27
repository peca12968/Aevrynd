#include <Aevrynd/PhysicsWorld3D.h>
#include <cmath>
#include <iostream>
#include <vector>

using namespace Aevrynd;

bool Near(float left,float right,float tolerance=.025f){return std::abs(left-right)<=tolerance;}

int main(){
    PhysicsWorld3D typeWorld;
    RigidBodySettings3D staticSettings;staticSettings.type=RigidBodyType3D::Static;staticSettings.transform.position={0,-.5f,0};staticSettings.colliderExtents={8,.5f,8};
    RigidBodySettings3D dynamicSettings;dynamicSettings.type=RigidBodyType3D::Dynamic;dynamicSettings.transform.position={0,4,0};dynamicSettings.colliderExtents={.5f,.5f,.5f};dynamicSettings.material.restitution=0;
    RigidBodySettings3D kinematicSettings;kinematicSettings.type=RigidBodyType3D::Kinematic;kinematicSettings.transform.position={4,2,0};kinematicSettings.colliderExtents={1,.25f,1};
    const RigidBodyHandle3D ground=typeWorld.CreateBody(staticSettings),falling=typeWorld.CreateBody(dynamicSettings),platform=typeWorld.CreateBody(kinematicSettings);
    const PhysicsWorldStats3D createdStats=typeWorld.Stats();
    if(!ground||!falling||!platform||createdStats.bodies!=3||createdStats.staticBodies!=1||createdStats.dynamicBodies!=1||createdStats.kinematicBodies!=1){std::cerr<<"Rigid body type creation failed\n";return 1;}
    for(int frame=0;frame<180;++frame)typeWorld.Step(1.f/60.f,2);
    const RigidBodyState3D* settled=static_cast<const PhysicsWorld3D&>(typeWorld).GetBody(falling);const RigidBodyState3D* unmovedPlatform=static_cast<const PhysicsWorld3D&>(typeWorld).GetBody(platform);
    if(!settled||!unmovedPlatform||!Near(settled->settings.transform.position.y,.5f)||!Near(settled->velocity.y,0,.001f)||!Near(unmovedPlatform->settings.transform.position.y,2.f)||typeWorld.Stats().contacts==0){std::cerr<<"Gravity settling or kinematic stability failed\n";return 2;}
    if(!settled->sleeping||typeWorld.Stats().sleepTransitions==0||typeWorld.Stats().sleepingBodies!=1){std::cerr<<"Sleep transition failed\n";return 3;}
    if(!typeWorld.ApplyImpulse(falling,{2,3,0})||static_cast<const PhysicsWorld3D&>(typeWorld).GetBody(falling)->sleeping){std::cerr<<"Impulse wake failed\n";return 4;}
    typeWorld.Step(1.f/60.f,2);const RigidBodyState3D* awakened=static_cast<const PhysicsWorld3D&>(typeWorld).GetBody(falling);
    if(!awakened||awakened->velocity.x<=0||awakened->settings.transform.position.x<=0){std::cerr<<"Woken body integration failed\n";return 5;}
    if(typeWorld.SetVelocity(ground,{1,0,0})||typeWorld.AddForce(platform,{1,0,0})||typeWorld.ApplyImpulse(platform,{1,0,0})){std::cerr<<"Body type mutation guards failed\n";return 6;}

    PhysicsWorld3D gravityWorld;gravityWorld.SetGravity({0,-12,0});RigidBodySettings3D freeSettings;freeSettings.type=RigidBodyType3D::Dynamic;freeSettings.transform.position={0,10,0};freeSettings.gravityScale=.5f;freeSettings.allowSleep=false;const RigidBodyHandle3D freeBody=gravityWorld.CreateBody(freeSettings);gravityWorld.Step(.1f,2);const RigidBodyState3D* freeState=static_cast<const PhysicsWorld3D&>(gravityWorld).GetBody(freeBody);
    if(!freeState||!Near(gravityWorld.Gravity().y,-12,.001f)||!Near(freeState->velocity.y,-.6f,.001f)||freeState->settings.transform.position.y>=10){std::cerr<<"Gravity scale integration failed\n";return 7;}

    PhysicsWorld3D layerWorld;RigidBodySettings3D filteredFloor=staticSettings;filteredFloor.layer=2;filteredFloor.collisionMask=2;filteredFloor.colliderExtents={3,.1f,3};filteredFloor.transform.position={0,0,0};layerWorld.CreateBody(filteredFloor);RigidBodySettings3D filteredBody=dynamicSettings;filteredBody.layer=1;filteredBody.collisionMask=1;filteredBody.transform.position={0,2,0};filteredBody.allowSleep=false;const RigidBodyHandle3D ignoredBody=layerWorld.CreateBody(filteredBody);for(int frame=0;frame<90;++frame)layerWorld.Step(1.f/60.f);const RigidBodyState3D* ignoredState=static_cast<const PhysicsWorld3D&>(layerWorld).GetBody(ignoredBody);
    if(!ignoredState||ignoredState->settings.transform.position.y>=-.5f||layerWorld.Stats().layerRejected==0){std::cerr<<"Layer mask rejection failed\n";return 8;}

    PhysicsWorld3D continuousWorld;RigidBodySettings3D thinFloor=staticSettings;thinFloor.colliderExtents={4,.1f,4};thinFloor.transform.position={0,0,0};thinFloor.material.restitution=0;continuousWorld.CreateBody(thinFloor);RigidBodySettings3D fastBody=dynamicSettings;fastBody.transform.position={0,8,0};fastBody.allowSleep=false;fastBody.gravityScale=0;fastBody.material.restitution=0;const RigidBodyHandle3D fastHandle=continuousWorld.CreateBody(fastBody);if(!continuousWorld.SetVelocity(fastHandle,{0,-160,0})){std::cerr<<"Fast body velocity setup failed\n";return 9;}continuousWorld.Step(.1f);const RigidBodyState3D* fastState=static_cast<const PhysicsWorld3D&>(continuousWorld).GetBody(fastHandle);const PhysicsWorldStats3D continuousStats=continuousWorld.Stats();
    if(!fastState||!Near(fastState->settings.transform.position.y,.6f)||!Near(fastState->velocity.y,0,.001f)||continuousStats.continuousCollisionTests==0||continuousStats.continuousContacts==0){std::cerr<<"Continuous collision detection failed\n";return 10;}

    if(!typeWorld.DestroyBody(platform)||typeWorld.DestroyBody(platform)||typeWorld.GetBody(platform)||typeWorld.SetTransform(platform,{})||typeWorld.Stats().kinematicBodies!=0){std::cerr<<"Body destruction or stale handle rejection failed\n";return 11;}

    PhysicsWorld3D memoryWorld;std::vector<RigidBodyHandle3D> handles;handles.reserve(4096);for(int index=0;index<4096;++index){RigidBodySettings3D settings;settings.type=(index%2)==0?RigidBodyType3D::Static:RigidBodyType3D::Dynamic;settings.transform.position={float(index)*2,0,0};settings.gravityScale=0;handles.push_back(memoryWorld.CreateBody(settings));}memoryWorld.Step(0);const ReusableMemoryStats peakMemory=memoryWorld.MemoryStats();for(const RigidBodyHandle3D handle:handles)if(!memoryWorld.DestroyBody(handle)){std::cerr<<"Bulk destruction failed\n";return 12;}const ReusableMemoryStats expandedMemory=memoryWorld.MemoryStats();const std::size_t releasedBytes=memoryWorld.TrimUnusedMemory();const ReusableMemoryStats trimmedMemory=memoryWorld.MemoryStats();
    if(!expandedMemory.reclaimableCapacityBytes||trimmedMemory.retainedCapacityBytes>expandedMemory.retainedCapacityBytes||trimmedMemory.reclaimableCapacityBytes||trimmedMemory.lastReleasedBytes!=releasedBytes||trimmedMemory.trimCount!=1){std::cerr<<"Physics workspace memory recovery failed peak="<<peakMemory.retainedCapacityBytes<<" expanded="<<expandedMemory.retainedCapacityBytes<<" reclaimable="<<expandedMemory.reclaimableCapacityBytes<<" released="<<releasedBytes<<" trimmed="<<trimmedMemory.retainedCapacityBytes<<" remaining="<<trimmedMemory.reclaimableCapacityBytes<<" last="<<trimmedMemory.lastReleasedBytes<<" trims="<<trimmedMemory.trimCount<<'\n';return 13;}
    const RigidBodyHandle3D reused=memoryWorld.CreateBody(dynamicSettings);memoryWorld.Step(0);if(!reused||!static_cast<const PhysicsWorld3D&>(memoryWorld).GetBody(reused)||memoryWorld.Stats().bodies!=1){std::cerr<<"Physics world reuse after trim failed\n";return 14;}

    const PhysicsWorldStats3D finalStats=typeWorld.Stats();std::cout<<"steps="<<finalStats.steps<<" substeps="<<finalStats.substeps<<" contacts="<<finalStats.contacts<<" sleeping="<<finalStats.sleepingBodies<<" continuous="<<continuousStats.continuousContacts<<" layerRejected="<<layerWorld.Stats().layerRejected<<" released="<<releasedBytes<<'\n';return 0;
}
