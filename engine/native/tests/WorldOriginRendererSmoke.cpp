#include <Aevrynd/Renderer.h>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;
namespace { bool Near(float a,float b){return std::abs(a-b)<.0001f;} }

int main(){
 DrawList draw;Atmosphere3D atmosphere{};atmosphere.enabled=true;atmosphere.cameraPosition={120,40,-80};atmosphere.heightFogBaseHeight=12;atmosphere.aerialPerspective.baseHeight=17;draw.SetAtmosphere(atmosphere);VolumetricCloudSettings3D clouds{};clouds.baseAltitude=1400;draw.SetVolumetricClouds(clouds);if(!draw.AddLocalReflectionProbe({EnvironmentLightingHandle3D{1},{100,0,-100},{140,40,-60},{120,20,-80},1}))return 1;
 const MeshHandle3D mesh{1};draw.MeshInstance(mesh,CreateTranslationMatrix({120,4,-80}),{}, {},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,0,false,1,0,false,0,ShadowCascadeSelection3D::Automatic,CreateTranslationMatrix({119,4,-80}),true);draw.SetOriginGeneration3D(1);
 if(!draw.ApplyWorldOriginShift3D({100,10,-50},2)||!draw.CameraCut3D()||draw.OriginGeneration3D()!=2)return 2;
 if(!Near(draw.Atmosphere().cameraPosition.x,20)||!Near(draw.Atmosphere().cameraPosition.y,30)||!Near(draw.Atmosphere().heightFogBaseHeight,2)||!Near(draw.Atmosphere().aerialPerspective.baseHeight,7)||!Near(draw.VolumetricClouds().baseAltitude,1390))return 3;
 const auto probes=draw.LocalReflectionProbes();if(probes.size()!=1||!Near(probes[0].minimum.x,0)||!Near(probes[0].maximum.y,30)||!Near(probes[0].capturePosition.z,-30))return 4;
 const Vec3 shiftedDynamic=TransformPoint(draw.meshInstances[0].transform,{});if(draw.meshInstances.size()!=1||draw.meshInstances[0].motionValid||!Near(shiftedDynamic.x,20)||!Near(shiftedDynamic.y,-6)||!Near(shiftedDynamic.z,-30))return 5;
 if(draw.ApplyWorldOriginShift3D({1,0,0},2)||draw.ApplyWorldOriginShift3D({NAN,0,0},3))return 6;
 DrawList staticDraw;staticDraw.MeshInstance(mesh,CreateTranslationMatrix({120,4,-80}),{}, {},MeshRenderLayer3D::Opaque,{},.25f,false,MaterialClass3D::Surface,0,false);staticDraw.SetStaticMeshSubmission3D(staticDraw.FreezeMeshInstances3D());staticDraw.meshInstances.clear();staticDraw.SetOriginGeneration3D(1);if(!staticDraw.PrepareWorldOriginShift3D({100,10,-50},2))return 7;const auto beforeCommit=staticDraw.EffectiveMeshInstances3D();if(beforeCommit.empty()||!Near(TransformPoint(beforeCommit[0].transform,{}).x,120)||!staticDraw.CommitPreparedWorldOriginShift3D())return 8;const auto afterCommit=staticDraw.EffectiveMeshInstances3D();const Vec3 shiftedStatic=TransformPoint(afterCommit[0].transform,{});if(afterCommit.empty()||!Near(shiftedStatic.x,20)||!Near(shiftedStatic.y,-6)||!Near(shiftedStatic.z,-30)||afterCommit[0].motionValid)return 9;
 const uint64_t staticRevision=staticDraw.StaticMeshSubmissionRevision3D();if(!staticDraw.PrepareWorldOriginShift3D({1,0,0},3))return 10;staticDraw.AbortPreparedWorldOriginShift3D();if(staticDraw.CommitPreparedWorldOriginShift3D()||staticDraw.OriginGeneration3D()!=2||staticDraw.StaticMeshSubmissionRevision3D()!=staticRevision)return 11;DrawList invalid;Atmosphere3D extreme{};extreme.cameraPosition={(std::numeric_limits<float>::max)(),0,0};invalid.SetAtmosphere(extreme);invalid.SetOriginGeneration3D(1);if(invalid.PrepareWorldOriginShift3D({-(std::numeric_limits<float>::max)(),0,0},2)||invalid.OriginGeneration3D()!=1||invalid.CameraCut3D())return 12;
 DrawList pure2D{DrawListCapacityProfile::Minimal2D};pure2D.Triangle({0,0},{1,0},{0,1},{1,1,1,1});if(!pure2D.ApplyWorldOriginShift3D({1000,0,1000},9)||pure2D.vertices.size()!=3)return 13;pure2D.Clear();if(pure2D.OriginGeneration3D()!=9||pure2D.CameraCut3D())return 14;
 std::cout<<"origin_generation=1 camera_cut=1 fog=1 cloud=1 probes=1 motion_reset=1 pure2d=1\n";
}
