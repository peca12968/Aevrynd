struct HlodSelectionCandidate {
 float3 center;float localBoundsRadius;
 float maximumScale;float geometricError;float transitionStart;float transitionEnd;
 uint sourceInstanceCount;uint flags;uint residentBytesLow;uint residentBytesHigh;
 uint previousStableMode;uint ditherSeed;uint2 padding;
};
struct HlodSelectionOutput {
 uint mode;uint packedCoverage;uint stableMode;uint residentBytesLow;uint residentBytesHigh;uint accepted;
};
StructuredBuffer<HlodSelectionCandidate> Candidates:register(t0);
RWStructuredBuffer<HlodSelectionOutput> Outputs:register(u0);
cbuffer SelectionConstants:register(b0){
 float3 cameraPosition;uint candidateCount;
 float3 cameraForward;uint validInput;
 float projectionScale;float inverseError;float nearPlane;float globalTransitionStart;
 float globalTransitionEnd;float hysteresis;uint settingsFlags;uint minimumSourceInstances;
 uint maximumResidentBytesLow;uint maximumResidentBytesHigh;uint2 padding;
}
bool Finite(float value){return !isnan(value)&&!isinf(value);}
bool Finite3(float3 value){return all(!isnan(value))&&all(!isinf(value));}
bool LessEqual64(uint2 value,uint2 limit){return value.y<limit.y||(value.y==limit.y&&value.x<=limit.x);}
uint2 Add64(uint2 left,uint2 right){uint low=left.x+right.x;return uint2(low,left.y+right.y+(low<left.x));}
uint2 Subtract64(uint2 left,uint2 right){return uint2(left.x-right.x,left.y-right.y-(left.x<right.x));}
[numthreads(1,1,1)] void HlodSelectionMain(uint3 id:SV_DispatchThreadID){
 if(any(id)||candidateCount==0)return;
 uint2 resident=uint2(0,0),maximumResident=uint2(maximumResidentBytesLow,maximumResidentBytesHigh);
 [loop]for(uint index=0;index<candidateCount;++index){
  HlodSelectionCandidate candidate=Candidates[index];
  HlodSelectionOutput output;output.mode=0;output.packedCoverage=255u|(uint(candidate.ditherSeed&7u)<<16);output.stableMode=candidate.previousStableMode;output.residentBytesLow=resident.x;output.residentBytesHigh=resident.y;output.accepted=0;
  float depth=dot(candidate.center-cameraPosition,cameraForward),radius=candidate.localBoundsRadius*candidate.maximumScale;
  bool safe=validInput!=0&&Finite3(candidate.center)&&Finite(candidate.maximumScale)&&Finite(depth)&&Finite(radius)&&depth-radius>nearPlane;
  float ratio=3.402823466e+38F;
  if(safe){ratio=candidate.geometricError*candidate.maximumScale*projectionScale*((settingsFlags&2u)!=0?inverseError:rcp(depth));safe=Finite(ratio);}
  uint2 bytes=uint2(candidate.residentBytesLow,candidate.residentBytesHigh);bool capacity=LessEqual64(resident,maximumResident)&&LessEqual64(bytes,Subtract64(maximumResident,resident));
  bool eligible=(settingsFlags&1u)!=0&&(candidate.flags&3u)==3u&&candidate.sourceInstanceCount>=minimumSourceInstances&&capacity;
  if(!safe||!eligible)output.stableMode=0;
  else{
   float low=max(candidate.transitionStart,globalTransitionStart),high=min(candidate.transitionEnd,globalTransitionEnd);
   if(high>low){
    float width=high-low,h=min(hysteresis,width*.5F);
    if((settingsFlags&4u)!=0){if(output.stableMode==1u)high-=h;else if(output.stableMode==2u)low+=h;}
    if(ratio>=high)output.stableMode=1;
    else if(ratio<=low){output.mode=2;output.packedCoverage=(255u<<8)|(uint(candidate.ditherSeed&7u)<<16);output.stableMode=2;resident=Add64(resident,bytes);output.accepted=1;}
    else{uint sourceCoverage=(uint)clamp(floor(((ratio-low)/(high-low))*255.0F+.5F),0.0F,255.0F);output.mode=1;output.packedCoverage=sourceCoverage|((255u-sourceCoverage)<<8)|(uint(candidate.ditherSeed&7u)<<16);output.stableMode=0;resident=Add64(resident,bytes);output.accepted=1;}
   }
  }
  output.residentBytesLow=resident.x;output.residentBytesHigh=resident.y;Outputs[index]=output;
 }
}
