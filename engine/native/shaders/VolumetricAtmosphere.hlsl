Texture2D<float4> previousAtmosphere : register(t0);
RWTexture2D<float4> currentAtmosphere : register(u0);

cbuffer AtmosphereConstants : register(b0)
{
    uint outputWidth;
    uint outputHeight;
    uint phase;
    uint historyValid;
    float historyWeight;
    float timeSeconds;
    float cloudCoverage;
    float cloudDensity;
    float cloudErosion;
    float cloudWindSpeed;
    float lightShaftIntensity;
    float lightShaftAnisotropy;
    float3 sunDirection;
    uint temporalSampleIndex;
};

float Hash21(float2 coordinate)
{
    coordinate=frac(coordinate*float2(123.34,456.21));
    coordinate+=dot(coordinate,coordinate+45.32);
    return frac(coordinate.x*coordinate.y);
}

float Noise2(float2 coordinate)
{
    const float2 cell=floor(coordinate);
    const float2 local=frac(coordinate);
    const float2 blend=local*local*(3.0-2.0*local);
    return lerp(lerp(Hash21(cell),Hash21(cell+float2(1,0)),blend.x),
                lerp(Hash21(cell+float2(0,1)),Hash21(cell+1),blend.x),blend.y);
}

float CloudNoise(float3 coordinate)
{
    float value=0;
    float amplitude=.55;
    float2 samplePoint=coordinate.xz*.0018+coordinate.y*.00037;
    [unroll] for(uint octave=0;octave<4;++octave)
    {
        value+=Noise2(samplePoint)*amplitude;
        samplePoint=samplePoint*2.07+17.31;
        amplitude*=.48;
    }
    return value;
}

float MiePhase(float cosine,float anisotropy)
{
    const float g=clamp(anisotropy,-.9,.9);
    const float denominator=max(.02,1.0+g*g-2.0*g*cosine);
    return (1.0-g*g)/(12.5663706*pow(denominator,1.5));
}

[numthreads(8,8,1)]
void VolumetricAtmosphereMain(uint3 id:SV_DispatchThreadID)
{
    if(id.x>=outputWidth||id.y>=outputHeight)return;
    const float2 uv=(float2(id.xy)+.5)/float2(outputWidth,outputHeight);
    const float2 screen=uv*2.0-1.0;
    const float3 ray=normalize(float3(screen.x*1.52,-screen.y*.86+.18,1.0));
    const float3 sun=normalize(sunDirection);
    const float wind=timeSeconds*cloudWindSpeed;
    float transmittance=1.0;
    float3 scattering=0;
    float integratedDensity=0;
    const float horizon=saturate(ray.y*3.5+.62);
    const float cloudRayValid=smoothstep(.015,.14,ray.y);
    const float entryDistance=ray.y>.001?970.0/ray.y:0;
    const float exitDistance=ray.y>.001?2300.0/ray.y:0;
    const float rayJitter=Hash21(float2(id.xy)+17.31+float(temporalSampleIndex)*23.71);
    [loop] for(uint sampleIndex=0;sampleIndex<16;++sampleIndex)
    {
        const float distance=lerp(entryDistance,exitDistance,(float(sampleIndex)+rayJitter)/16.0);
        const float3 samplePosition=float3(0,80,0)+ray*distance+float3(wind*.72,0,wind*.34);
        const float heightProfile=saturate((samplePosition.y-1050.0)/420.0)*saturate((2380.0-samplePosition.y)/680.0);
        const float broad=CloudNoise(samplePosition);
        const float detail=CloudNoise(samplePosition*2.31+31.7);
        const float density=saturate((broad-cloudCoverage-cloudErosion*detail)*4.1)*heightProfile*cloudDensity*cloudRayValid;
        const float opticalDepth=density*.105;
        const float stepOpacity=1.0-exp(-opticalDepth);
        const float sunAmount=saturate(dot(sun,float3(0,1,0))*.72+.28);
        const float3 cloudLight=lerp(float3(.18,.22,.3),float3(1.0,.87,.68),sunAmount);
        scattering+=transmittance*stepOpacity*cloudLight;
        transmittance*=1.0-stepOpacity;
        integratedDensity+=opticalDepth;
    }
    const float shaftAlignment=saturate(dot(ray,sun)*.5+.5);
    const float shaft=MiePhase(dot(ray,sun),lightShaftAnisotropy)*lightShaftIntensity*
                      (1.0-exp(-integratedDensity*.42))*horizon;
    const float3 dayColor=float3(.34,.57,.91);
    const float3 duskColor=float3(.78,.31,.16);
    const float3 nightColor=float3(.025,.045,.105);
    const float3 skyColor=phase==0?dayColor:(phase==1?duskColor:nightColor);
    const float phaseBrightness=phase==0?1.0:(phase==1?.72:.24);
    float3 color=skyColor*(.16+.84*horizon)*(1.0-transmittance)*.42+scattering*phaseBrightness;
    color+=float3(1.0,.78,.52)*shaft*shaftAlignment*phaseBrightness;
    const float alpha=saturate(1.0-transmittance+shaft*.2);
    const float4 current=float4(max(0,color),alpha);
    const float4 history=previousAtmosphere.Load(int3(id.xy,0));
    currentAtmosphere[id.xy]=historyValid!=0?lerp(current,history,saturate(historyWeight)):current;
}
