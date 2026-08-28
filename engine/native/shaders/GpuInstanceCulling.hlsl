struct GpuMeshInstance {row_major float4x4 transform;float4 normalRows[3];uint tint;uint surface;uint textureData;};
struct GpuCullGroup {uint inputOffset;uint instanceCount;uint outputOffset;uint commandIndex;float3 boundsCenter;float _pad0;float3 boundsExtents;float maximumDistance;};
StructuredBuffer<GpuMeshInstance> InputInstances:register(t0);
StructuredBuffer<GpuCullGroup> Groups:register(t1);
RWStructuredBuffer<GpuMeshInstance> OutputInstances:register(u0);
RWByteAddressBuffer Arguments:register(u1);
cbuffer CullConstants:register(b0){row_major float4x4 viewProjection;float3 cameraPosition;uint groupCount;uint totalCandidates;uint distanceEnabled;uint2 _pad1;}

[numthreads(64,1,1)] void GpuInstanceCullingClearMain(uint3 id:SV_DispatchThreadID){
 if(id.x>=groupCount)return;GpuCullGroup group=Groups[id.x];Arguments.Store(group.commandIndex*72+52,0);
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
 return clipCenter.x+clipExtent.x>=-clipCenter.w-clipExtent.w&&clipCenter.x-clipExtent.x<=clipCenter.w+clipExtent.w&&clipCenter.y+clipExtent.y>=-clipCenter.w-clipExtent.w&&clipCenter.y-clipExtent.y<=clipCenter.w+clipExtent.w&&clipCenter.z+clipExtent.z>=0&&clipCenter.z-clipExtent.z<=clipCenter.w+clipExtent.w;
}
[numthreads(64,1,1)] void GpuInstanceCullingMain(uint3 groupId:SV_GroupID,uint3 lane:SV_GroupThreadID){
 if(groupId.x>=groupCount)return;uint groupIndex=groupId.x;GpuCullGroup group=Groups[groupIndex];
 for(uint localIndex=lane.x;localIndex<group.instanceCount;localIndex+=64){GpuMeshInstance instance=InputInstances[group.inputOffset+localIndex];if(!Visible(instance,group))continue;
  uint outputIndex;Arguments.InterlockedAdd(group.commandIndex*72+52,1,outputIndex);OutputInstances[group.outputOffset+outputIndex]=instance;}
}
