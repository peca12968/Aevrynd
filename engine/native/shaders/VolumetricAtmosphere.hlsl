Texture2D<float4> previousAtmosphere : register(t0);
Texture2D<float> sceneDepthTexture : register(t1);
Texture2D<float2> previousSegmentTexture : register(t2);
ByteAddressBuffer blueNoiseBytes : register(t3);
RWTexture2D<float4> currentAtmosphere : register(u0);
RWTexture2D<float2> currentSegmentTexture : register(u1);
#include "WorldRayReconstruction.hlsli"
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
RWStructuredBuffer<uint> temporalCounters : register(u2);
#endif

cbuffer AtmosphereConstants : register(b0)
{
    uint outputWidth; uint outputHeight; uint fullWidth; uint fullHeight;
    float2 viewportOrigin; float2 viewportExtent;
    uint phase; uint historyValid;
    float historyWeight; float timeSeconds; float cloudCoverage; float cloudDensity;
    float cloudErosion; float cloudWindSpeed; float lightShaftIntensity; float lightShaftAnisotropy;
    uint temporalDiagnosticsEnabled;
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
    uint testMutation;
#else
    uint productionMutationPadding;
#endif
    float3 sunDirection; uint temporalSampleIndex;
    row_major float4x4 inverseViewProjection;
    float3 cameraPosition; float cloudBaseAltitude;
    float cloudThickness; uint sceneDepthValid; uint projectionOrthographic; uint previousProjectionOrthographic;
    row_major float4x4 previousViewProjection;
    uint cloudSampleCount; uint lightShaftSampleCount; uint cloudShadowSampleCount; uint cloudShadowsEnabled;
    float cloudMaximumDistance; float lightShaftMaximumDistance; float cloudShadowStrength; float lightShaftShadowStrength;
    float heightFogDensity; float heightFogFalloff; float heightFogBaseHeight; float heightFogMaximumOpacity;
    float3 sunRadiance; float sunRadianceIntensity;
    float3 ambientRadiance; float temporalDeltaTime;
    uint cloudBaseNoiseSeed; uint cloudDetailNoiseSeed; float cloudBaseNoiseRotation; float cloudDetailNoiseRotation;
    float4 constantPadding[5];
};

uint HashLattice(int3 coordinate,uint seed)
{
    uint value=asuint(coordinate.x)*0x8da6b343u^asuint(coordinate.y)*0xd8163841u^asuint(coordinate.z)*0xcb1ab31fu^seed;
    value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;
    return value;
}

float ValueNoise3D(float3 coordinate,uint seed)
{
    const int3 cell=int3(floor(coordinate));const float3 local=frac(coordinate);
    const float3 blend=local*local*(3.0-2.0*local);float samples[8];
    [unroll] for(uint z=0;z<2;++z)[unroll] for(uint y=0;y<2;++y)[unroll] for(uint x=0;x<2;++x)
        samples[x+y*2+z*4]=float(HashLattice(cell+int3(x,y,z),seed)&0x00ffffffu)/16777215.0;
    const float x00=lerp(samples[0],samples[1],blend.x),x10=lerp(samples[2],samples[3],blend.x);
    const float x01=lerp(samples[4],samples[5],blend.x),x11=lerp(samples[6],samples[7],blend.x);
    return lerp(lerp(x00,x10,blend.y),lerp(x01,x11,blend.y),blend.z);
}

float3 RotateNoise(float3 value,float angle,uint octave)
{
    const float a=angle+float(octave)*1.04719755,c=cos(a),s=sin(a);
    const float3 y=float3(c*value.x-s*value.z,value.y,s*value.x+c*value.z);
    return float3(y.x,c*y.y-s*y.z,s*y.y+c*y.z);
}

float Fbm3D(float3 coordinate,uint seed,float rotation,uint octaves)
{
    float value=0,weight=0,amplitude=.55;
    [loop] for(uint octave=0;octave<octaves;++octave)
    {value+=ValueNoise3D(RotateNoise(coordinate,rotation,octave),seed+octave*0x9e3779b9u)*amplitude;weight+=amplitude;coordinate=coordinate*2.03+float3(17.1,31.7,11.9);amplitude*=.48;}
    return value/max(weight,1e-5);
}

#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
float LegacyHash21(float2 coordinate)
{
    coordinate=frac(coordinate*float2(123.34,456.21));coordinate+=dot(coordinate,coordinate+45.32);return frac(coordinate.x*coordinate.y);
}
float LegacyNoise2(float2 coordinate)
{
    const float2 cell=floor(coordinate),local=frac(coordinate),blend=local*local*(3.0-2.0*local);
    return lerp(lerp(LegacyHash21(cell),LegacyHash21(cell+float2(1,0)),blend.x),lerp(LegacyHash21(cell+float2(0,1)),LegacyHash21(cell+1),blend.x),blend.y);
}
float LegacyPlanarCloudNoise(float3 coordinate)
{
    float value=0,amplitude=.55;float2 samplePoint=coordinate.xz*.0018+coordinate.y*.00037;
    [unroll] for(uint octave=0;octave<4;++octave){value+=LegacyNoise2(samplePoint)*amplitude;samplePoint=samplePoint*2.07+17.31;amplitude*=.48;}
    return value;
}
float LegacyMiePhase(float cosine,float anisotropy)
{
    const float g=clamp(anisotropy,-.9,.9),denominator=max(.02,1.0+g*g-2.0*g*cosine);
    return (1.0-g*g)/(12.5663706*pow(denominator,1.5));
}
#endif

float CloudField(float3 worldPosition)
{
    const float3 advected=worldPosition+float3(.72,0,.34)*(timeSeconds*cloudWindSpeed);
    float base=Fbm3D(advected*.00125,cloudBaseNoiseSeed,cloudBaseNoiseRotation,4);
    float detail=Fbm3D(advected*.0047+29.3,cloudDetailNoiseSeed,cloudDetailNoiseRotation,3);
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
    if(testMutation==3){base=LegacyPlanarCloudNoise(advected);detail=LegacyPlanarCloudNoise(advected*2.31+31.7);}
#endif
    const float h=saturate((worldPosition.y-cloudBaseAltitude)/max(1.0,cloudThickness));
    const float profile=smoothstep(0.0,.16,h)*smoothstep(0.0,.28,1.0-h);
    const float threshold=cloudCoverage+cloudErosion*(detail-.5);
    return smoothstep(threshold,threshold+.18,base)*profile*cloudDensity;
}

float HenyeyGreenstein(float cosine,float anisotropy)
{
    const float g=clamp(anisotropy,-.8,.85),g2=g*g;
    return (1.0-g2)/(12.5663706144*pow(max(1e-6,1.0+g2-2.0*g*clamp(cosine,-1.0,1.0)),1.5));
}

float CloudShadowField(float3 worldPosition)
{
    const float3 advected=worldPosition+float3(.72,0,.34)*(timeSeconds*cloudWindSpeed);
    const float base=ValueNoise3D(RotateNoise(advected*.00125,cloudBaseNoiseRotation,0),cloudBaseNoiseSeed);
    const float h=saturate((worldPosition.y-cloudBaseAltitude)/max(1.0,cloudThickness));
    const float profile=smoothstep(0.0,.16,h)*smoothstep(0.0,.28,1.0-h);
    return smoothstep(cloudCoverage,cloudCoverage+.22,base)*profile*cloudDensity;
}

float BlueR2Jitter(uint2 pixel,uint frameIndex)
{
    const uint byteIndex=(pixel.x&63u)+((pixel.y&63u)<<6),packed=blueNoiseBytes.Load(byteIndex&~3u);
    const float blue=float((packed>>((byteIndex&3u)*8u))&255u)/256.0;
    return frac(blue+float(frameIndex)*.7548776662466927+float(pixel.x)*.5698402909980532+float(pixel.y)*.4384471871911697);
}

float CloudSunTransmittance(float3 p,float3 sun,uint shadowSamples)
{
    if(cloudShadowsEnabled==0||cloudShadowStrength<=0.0)return 1.0;
    const uint count=clamp(shadowSamples,1u,8u);const float stepLength=max(8.0,cloudThickness/(float(count)*3.0));float depth=0;
    [loop] for(uint index=0;index<count;++index)depth+=CloudShadowField(p+sun*stepLength*(float(index)+.5))*.00135*stepLength;
    return lerp(1.0,exp(-min(depth,80.0)),saturate(cloudShadowStrength));
}

float HeightFogExtinction(float height){return heightFogDensity*exp(clamp(-heightFogFalloff*(height-heightFogBaseHeight),-20.0,20.0));}

[numthreads(8,8,1)]
void VolumetricAtmosphereMain(uint3 id:SV_DispatchThreadID)
{
    if(id.x>=outputWidth||id.y>=outputHeight)return;
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
    // 僅供 upscale 負向 gate；固定 Nyquist pattern 不讀場景、時間或 history。
    if(testMutation==9)
    {
        const bool high=((id.x^id.y)&1u)!=0;
        currentAtmosphere[id.xy]=high?float4(.36,.18,.06,.62):float4(.02,.04,.08,.12);
        currentSegmentTexture[id.xy]=0;
        return;
    }
#endif
    const float2 uv=(float2(id.xy)+.5)/float2(outputWidth,outputHeight);float3 rayOrigin,ray;
    const bool rayValid=AevryndReconstructWorldRay(uv,inverseViewProjection,cameraPosition,projectionOrthographic!=0,rayOrigin,ray);
    const uint2 depthPixel=min(uint2(viewportOrigin+uv*viewportExtent),uint2(fullWidth-1,fullHeight-1));
    const float hardwareDepth=sceneDepthValid!=0?sceneDepthTexture.Load(int3(depthPixel,0)):1.0;float3 surfaceWorld;
    const bool surfaceValid=hardwareDepth<.999999&&AevryndReconstructWorldPosition(uv,hardwareDepth,inverseViewProjection,surfaceWorld);
    const float sceneDistance=surfaceValid?max(0.0,dot(surfaceWorld-rayOrigin,ray)):1e30;
    const float inverseRayY=abs(ray.y)>1e-6?rcp(ray.y):0.0,slab0=(cloudBaseAltitude-rayOrigin.y)*inverseRayY,slab1=(cloudBaseAltitude+cloudThickness-rayOrigin.y)*inverseRayY;
    const float layerEntry=min(slab0,slab1),layerExit=max(slab0,slab1);
    float pathCap=cloudMaximumDistance,pathFade=smoothstep(0.0,clamp(cloudThickness*.12,32.0,256.0),cloudMaximumDistance-max(0.0,layerEntry));
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
    if(testMutation==4){pathCap=1e30;pathFade=1.0;}
#endif
    const float cloudStart=max(0.0,layerEntry),cloudEnd=min(min(layerExit,sceneDistance),pathCap),grazing=smoothstep(.004,.032,abs(ray.y));
    const bool cloudValid=rayValid&&abs(ray.y)>1e-6&&cloudDensity>0.0&&grazing>0.0&&cloudEnd>cloudStart+1e-3;
    const float shaftEnd=min(sceneDistance,lightShaftMaximumDistance);const bool shaftValid=rayValid&&lightShaftIntensity>0.0&&heightFogDensity>0.0&&shaftEnd>1e-3;
    const bool segmentValid=cloudValid||shaftValid;
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
    const bool diagnosticRepresentative=temporalDiagnosticsEnabled!=0&&(id.x&7)==0&&(id.y&7)==0;
    if(!segmentValid&&diagnosticRepresentative)InterlockedAdd(temporalCounters[2],1);
#endif
    const float3 sun=normalize(sunDirection);const float phaseValue=HenyeyGreenstein(dot(ray,sun),lightShaftAnisotropy);const float jitter=BlueR2Jitter(id.xy,temporalSampleIndex);
    const float phaseSunScale=phase==0?1.0:(phase==1?.58:.08),phaseAmbientScale=phase==0?1.0:(phase==1?.62:.14);
    float cloudT=1.0;float3 cloudL=0;float integratedCloudOpticalDepth=0;
    if(cloudValid)
    {
        const uint count=clamp(cloudSampleCount,4u,32u);const float segmentLength=cloudEnd-cloudStart,physicalStep=segmentLength/float(count);
        [loop] for(uint index=0;index<count;++index)
        {
            const float distance=cloudStart+physicalStep*(float(index)+jitter);const float3 p=rayOrigin+ray*distance;const float density=CloudField(p)*grazing*pathFade;
            const float sigmaT=density*.00135,sigmaS=sigmaT*.88;
            float opticalDepth=sigmaT*physicalStep;
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
            if(testMutation==6)opticalDepth=density*.105*segmentLength/max(1.0,cloudThickness);
#endif
            const float stepT=exp(-min(opticalDepth,80.0));const float scatter=sigmaT>1e-7?sigmaS/sigmaT*(1.0-stepT):0.0;
            const float sunT=CloudSunTransmittance(p,sun,cloudShadowSampleCount);float3 incident=ambientRadiance*phaseAmbientScale+sunRadiance*sunRadianceIntensity*phaseSunScale*sunT*phaseValue;
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
            // 中文：只歸零夜間雲層入射光，保留消光累積以驗證 opacity 路徑沒有被旁路。
            if(testMutation==8)incident=0.0;
#endif
            cloudL+=cloudT*scatter*incident;cloudT*=stepT;integratedCloudOpticalDepth+=opticalDepth;
        }
    }
    float fogT=1.0;float3 shaftL=0;
    if(shaftValid)
    {
        const uint count=clamp(lightShaftSampleCount,2u,16u);const float stepLength=shaftEnd/float(count);
        [loop] for(uint index=0;index<count;++index)
        {
            const float distance=stepLength*(float(index)+jitter);const float3 p=rayOrigin+ray*distance;const float sigmaT=HeightFogExtinction(p.y),sigmaS=sigmaT*.9;
            const float stepT=exp(-min(sigmaT*stepLength,80.0)),scatter=sigmaT>1e-7?sigmaS/sigmaT*(1.0-stepT):0.0;
            const float cloudToSun=CloudSunTransmittance(p,sun,cloudShadowSampleCount),visibility=lerp(1.0,cloudToSun,saturate(lightShaftShadowStrength));
            shaftL+=fogT*scatter*sunRadiance*sunRadianceIntensity*phaseSunScale*visibility*phaseValue*lightShaftIntensity;fogT*=stepT;
        }
        const float rawOpacity=1.0-fogT,limitedOpacity=min(rawOpacity,saturate(heightFogMaximumOpacity));
        if(rawOpacity>1e-7)shaftL*=limitedOpacity/rawOpacity;
        fogT=1.0-limitedOpacity;
    }
    const float3 conservativeColor=shaftL+fogT*cloudL;const float conservativeAlpha=saturate(1.0-fogT*cloudT);
    float3 color=conservativeColor;float alpha=conservativeAlpha;
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
    if(testMutation==5){const float alignment=saturate(dot(ray,sun)*.5+.5),horizon=saturate(ray.y*3.5+.62),legacyShaft=LegacyMiePhase(dot(ray,sun),lightShaftAnisotropy)*lightShaftIntensity*(1.0-exp(-integratedCloudOpticalDepth*.42))*horizon;color=cloudL+float3(1.0,.78,.52)*legacyShaft*alignment*phaseSunScale;alpha=saturate(1.0-cloudT+legacyShaft*.2);}
#endif
    const float4 current=segmentValid?float4(max(0,color),alpha):0;
    const float segmentStart=cloudValid?min(cloudStart,shaftValid?0.0:cloudStart):0.0,segmentEnd=max(cloudValid?cloudEnd:0.0,shaftValid?shaftEnd:0.0);
    currentSegmentTexture[id.xy]=segmentValid?float2(segmentStart,segmentEnd):0;
    bool historyAccepted=false;float4 history=0;
    if(segmentValid&&historyValid!=0&&projectionOrthographic==previousProjectionOrthographic)
    {
        float windReprojectionSign=1.0;
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
        if(testMutation==7)windReprojectionSign=-1.0;
#endif
        // CloudField(p,t)=F(p+w*t)；前一幀查 p+w*(tc-tp) 才能追到相同 feature。
        const float3 representativeWorld=rayOrigin+ray*((segmentStart+segmentEnd)*.5)+float3(.72,0,.34)*(windReprojectionSign*temporalDeltaTime*cloudWindSpeed);
        const float4 previousClip=mul(previousViewProjection,float4(representativeWorld,1));
        if(abs(previousClip.w)>1e-6)
        {
            const float2 previousNdc=previousClip.xy/previousClip.w,previousUv=float2(previousNdc.x*.5+.5,.5-previousNdc.y*.5);
            if(all(previousUv>=0.0)&&all(previousUv<=1.0))
            {
                const uint2 previousPixel=min(uint2(previousUv*float2(outputWidth,outputHeight)),uint2(outputWidth-1,outputHeight-1));const float2 previousSegment=previousSegmentTexture.Load(int3(previousPixel,0));
                const float tolerance=max(4.0,(segmentEnd-segmentStart)*.08);historyAccepted=previousSegment.y>previousSegment.x&&abs(previousSegment.x-segmentStart)<=tolerance&&abs(previousSegment.y-segmentEnd)<=tolerance;
                if(historyAccepted)history=previousAtmosphere.Load(int3(previousPixel,0));
            }
        }
    }
#if AEVRYND_VOLUMETRIC_DIAGNOSTICS
    if(segmentValid&&historyValid!=0&&diagnosticRepresentative){if(historyAccepted)InterlockedAdd(temporalCounters[0],1);else InterlockedAdd(temporalCounters[1],1);}
#endif
    currentAtmosphere[id.xy]=segmentValid?(historyAccepted?lerp(current,history,saturate(historyWeight)):current):0;
}
