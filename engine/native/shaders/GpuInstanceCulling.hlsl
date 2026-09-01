struct GpuMeshInstance {row_major float4x4 transform;float4 normalRows[3];uint tint;uint surface;uint textureData;uint objectToken;float occlusionOpacity;uint occlusionSeed0;uint occlusionSeed1;};
struct GpuCullGroup {uint inputOffset;uint instanceCount;uint outputOffset;uint commandIndex;float3 boundsCenter;uint occluder;float3 boundsExtents;float maximumDistance;};
StructuredBuffer<GpuMeshInstance> InputInstances:register(t0);
StructuredBuffer<GpuCullGroup> Groups:register(t1);
Texture2D<float> HierarchicalDepth:register(t2);
RWStructuredBuffer<GpuMeshInstance> OutputInstances:register(u0);
RWByteAddressBuffer Arguments:register(u1);
cbuffer CullConstants:register(b0){row_major float4x4 viewProjection;float3 cameraPosition;uint groupCount;uint totalCandidates;uint distanceEnabled;uint hzbEnabled;uint hzbWidth;uint hzbHeight;uint hzbMipCount;float hzbDepthBias;uint hzbCounterOffset;uint2 _pad1;}

[numthreads(64,1,1)] void GpuInstanceCullingClearMain(uint3 id:SV_DispatchThreadID){
 if(id.x>=groupCount)return;GpuCullGroup group=Groups[id.x];Arguments.Store(group.commandIndex*72+52,0);if(id.x==0)Arguments.Store(hzbCounterOffset,0);
}
bool HzbVisible(GpuMeshInstance instance,GpuCullGroup group){
 if(hzbEnabled==0||group.occluder!=0||hzbWidth==0||hzbHeight==0||hzbMipCount==0)return true;
 float3 minimumLocal=group.boundsCenter-group.boundsExtents,maximumLocal=group.boundsCenter+group.boundsExtents;
 float2 minimumNdc=float2(1e30,1e30),maximumNdc=float2(-1e30,-1e30);float nearestDepth=1.0;
 [unroll]for(uint corner=0;corner<8;++corner){float3 local=float3((corner&1)?maximumLocal.x:minimumLocal.x,(corner&2)?maximumLocal.y:minimumLocal.y,(corner&4)?maximumLocal.z:minimumLocal.z);float4 clip=mul(viewProjection,mul(instance.transform,float4(local,1)));if(any(isnan(clip))||any(isinf(clip))||clip.w<=1e-5||clip.z<0)return true;float3 ndc=clip.xyz/clip.w;if(any(isnan(ndc))||any(isinf(ndc)))return true;minimumNdc=min(minimumNdc,ndc.xy);maximumNdc=max(maximumNdc,ndc.xy);nearestDepth=min(nearestDepth,ndc.z);}
 float2 minimumPixel=floor((minimumNdc*.5+.5)*float2(hzbWidth,hzbHeight))-1.0;float2 maximumPixel=ceil((maximumNdc*.5+.5)*float2(hzbWidth,hzbHeight))+1.0;
 minimumPixel=clamp(minimumPixel,0.0,float2(hzbWidth-1,hzbHeight-1));maximumPixel=clamp(maximumPixel,0.0,float2(hzbWidth-1,hzbHeight-1));
 uint maximumSpan=max((uint)max(1.0,maximumPixel.x-minimumPixel.x+1.0),(uint)max(1.0,maximumPixel.y-minimumPixel.y+1.0));uint mip=maximumSpan>1?firstbithigh(maximumSpan-1)+1:0;mip=min(mip,hzbMipCount-1);if(mip==0)return true;
 uint2 mipSize=max(uint2(1,1),uint2(hzbWidth,hzbHeight)>>mip);uint2 minimumTexel=min(uint2(minimumPixel*float2(mipSize)/float2(hzbWidth,hzbHeight)),mipSize-1);uint2 maximumTexel=min(uint2(maximumPixel*float2(mipSize)/float2(hzbWidth,hzbHeight)),mipSize-1);
 while((maximumTexel.x-minimumTexel.x>1||maximumTexel.y-minimumTexel.y>1)&&mip+1<hzbMipCount){++mip;mipSize=max(uint2(1,1),uint2(hzbWidth,hzbHeight)>>mip);minimumTexel=min(uint2(minimumPixel*float2(mipSize)/float2(hzbWidth,hzbHeight)),mipSize-1);maximumTexel=min(uint2(maximumPixel*float2(mipSize)/float2(hzbWidth,hzbHeight)),mipSize-1);}
 if(maximumTexel.x-minimumTexel.x>1||maximumTexel.y-minimumTexel.y>1)return true;
 float maximumDepth=0.0;[loop]for(uint y=minimumTexel.y;y<=maximumTexel.y;++y)[loop]for(uint x=minimumTexel.x;x<=maximumTexel.x;++x)maximumDepth=max(maximumDepth,HierarchicalDepth.Load(int3(uint2(x,y),mip-1)));
 bool visible=!(nearestDepth>maximumDepth+hzbDepthBias);if(!visible){uint previous;Arguments.InterlockedAdd(hzbCounterOffset,1,previous);}return visible;
}
bool Visible(GpuMeshInstance instance,GpuCullGroup group){
 float3 worldCenter=mul(instance.transform,float4(group.boundsCenter,1)).xyz;
 float3 ex=abs(mul((float3x3)instance.transform,float3(group.boundsExtents.x,0,0)));
 float3 ey=abs(mul((float3x3)instance.transform,float3(0,group.boundsExtents.y,0)));
 float3 ez=abs(mul((float3x3)instance.transform,float3(0,0,group.boundsExtents.z)));
 float3 worldExtent=ex+ey+ez;
 float maximumCenterDistance=group.maximumDistance+length(worldExtent);
 if(distanceEnabled!=0&&group.maximumDistance>0&&dot(worldCenter-cameraPosition,worldCenter-cameraPosition)>maximumCenterDistance*maximumCenterDistance)return false;
 float4 clipCenter=mul(viewProjection,float4(worldCenter,1));
 float4 clipExtent=float4(dot(abs(viewProjection[0].xyz),worldExtent),dot(abs(viewProjection[1].xyz),worldExtent),dot(abs(viewProjection[2].xyz),worldExtent),dot(abs(viewProjection[3].xyz),worldExtent));
 bool frustum=clipCenter.x+clipExtent.x>=-clipCenter.w-clipExtent.w&&clipCenter.x-clipExtent.x<=clipCenter.w+clipExtent.w&&clipCenter.y+clipExtent.y>=-clipCenter.w-clipExtent.w&&clipCenter.y-clipExtent.y<=clipCenter.w+clipExtent.w&&clipCenter.z+clipExtent.z>=0&&clipCenter.z-clipExtent.z<=clipCenter.w+clipExtent.w;return frustum&&HzbVisible(instance,group);
}
[numthreads(64,1,1)] void GpuInstanceCullingMain(uint3 groupId:SV_GroupID,uint3 lane:SV_GroupThreadID){
 if(groupId.x>=groupCount)return;uint groupIndex=groupId.x;GpuCullGroup group=Groups[groupIndex];
 for(uint localIndex=lane.x;localIndex<group.instanceCount;localIndex+=64){GpuMeshInstance instance=InputInstances[group.inputOffset+localIndex];if(!Visible(instance,group))continue;
  uint outputIndex;Arguments.InterlockedAdd(group.commandIndex*72+52,1,outputIndex);OutputInstances[group.outputOffset+outputIndex]=instance;}
}
