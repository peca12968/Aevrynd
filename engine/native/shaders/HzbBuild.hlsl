Texture2D<float> SourceDepth : register(t0);
RWTexture2D<float> DestinationDepth0 : register(u0);
RWTexture2D<float> DestinationDepth1 : register(u1);
RWTexture2D<float> DestinationDepth2 : register(u2);
RWTexture2D<float> DestinationDepth3 : register(u3);
cbuffer HzbBuildConstants : register(b0) { uint2 sourceSize; uint2 destinationSize0; uint2 destinationSize1; uint copyFirst; uint outputCount; }
float ReduceMaximum(uint2 outputCoordinate,uint shift){const uint block=1u<<shift;const uint2 base=outputCoordinate*block;float result=0.0;[loop]for(uint y=0;y<block;++y)[loop]for(uint x=0;x<block;++x)result=max(result,SourceDepth.Load(int3(min(base+uint2(x,y),sourceSize-1),0)));return result;}
[numthreads(8,8,1)] void HzbBuildMain(uint3 id:SV_DispatchThreadID){const uint firstShift=copyFirst!=0?0:1;const uint2 destinationSize2=max(uint2(1,1),sourceSize>>(firstShift+2));const uint2 destinationSize3=max(uint2(1,1),sourceSize>>(firstShift+3));if(all(id.xy<destinationSize0))DestinationDepth0[id.xy]=ReduceMaximum(id.xy,firstShift);if(outputCount>1&&all(id.xy<destinationSize1))DestinationDepth1[id.xy]=ReduceMaximum(id.xy,firstShift+1);if(outputCount>2&&all(id.xy<destinationSize2))DestinationDepth2[id.xy]=ReduceMaximum(id.xy,firstShift+2);if(outputCount>3&&all(id.xy<destinationSize3))DestinationDepth3[id.xy]=ReduceMaximum(id.xy,firstShift+3);}
