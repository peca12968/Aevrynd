#include <Aevrynd/Renderer.h>
#include <cmath>
#include <iostream>

using namespace Aevrynd;
namespace { bool Near(float a,float b){return std::abs(a-b)<.0001f;} }

int main(){
 DrawList draw;Atmosphere3D atmosphere{};atmosphere.enabled=true;atmosphere.cameraPosition={120,40,-80};atmosphere.heightFogBaseHeight=12;draw.SetAtmosphere(atmosphere);VolumetricCloudSettings3D clouds{};clouds.baseAltitude=1400;draw.SetVolumetricClouds(clouds);if(!draw.AddLocalReflectionProbe({EnvironmentLightingHandle3D{1},{100,0,-100},{140,40,-60},{120,20,-80},1}))return 1;
 const MeshHandle3D mesh{1};draw.MeshInstance(mesh,CreateTranslationMatrix({120,4,-80}),{}, {},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,0,false,1,0,false,0,ShadowCascadeSelection3D::Automatic,CreateTranslationMatrix({119,4,-80}),true);draw.SetOriginGeneration3D(1);
 if(!draw.ApplyWorldOriginShift3D({100,10,-50},2)||!draw.CameraCut3D()||draw.OriginGeneration3D()!=2)return 2;
 if(!Near(draw.Atmosphere().cameraPosition.x,20)||!Near(draw.Atmosphere().cameraPosition.y,30)||!Near(draw.Atmosphere().heightFogBaseHeight,2)||!Near(draw.VolumetricClouds().baseAltitude,1390))return 3;
 const auto probes=draw.LocalReflectionProbes();if(probes.size()!=1||!Near(probes[0].minimum.x,0)||!Near(probes[0].maximum.y,30)||!Near(probes[0].capturePosition.z,-30))return 4;
 if(draw.meshInstances.size()!=1||draw.meshInstances[0].motionValid)return 5;
 if(draw.ApplyWorldOriginShift3D({1,0,0},2)||draw.ApplyWorldOriginShift3D({NAN,0,0},3))return 6;
 DrawList pure2D{DrawListCapacityProfile::Minimal2D};pure2D.Triangle({0,0},{1,0},{0,1},{1,1,1,1});if(!pure2D.ApplyWorldOriginShift3D({1000,0,1000},9)||pure2D.vertices.size()!=3)return 7;pure2D.Clear();if(pure2D.OriginGeneration3D()!=9||pure2D.CameraCut3D())return 8;
 std::cout<<"origin_generation=1 camera_cut=1 fog=1 cloud=1 probes=1 motion_reset=1 pure2d=1\n";
}
