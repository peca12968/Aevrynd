// 獨立 coverage cell 與連續 domain warp 候選，不供 production 使用。
float candidateFrequency=.00022;
float candidateThreshold=.44;
float candidateMassScale=1;
float candidateWarp=.4;
float candidateAsymmetry=.15;
float CandidateCloudDensity(float3 worldPosition)
{
    if(!isfinite(worldPosition.x)||!isfinite(worldPosition.y)||!isfinite(worldPosition.z)||
       !isfinite(cloudThickness)||!isfinite(cloudDensity)||!isfinite(timeSeconds)||
       !isfinite(candidateMassScale)||!isfinite(candidateFrequency)||!isfinite(candidateThreshold)||
       !isfinite(candidateWarp)||!isfinite(candidateAsymmetry)||
       cloudThickness<=0||cloudDensity<=0||candidateMassScale<=0||candidateFrequency<=0)return 0;
    const float h=(worldPosition.y-cloudBaseAltitude)/cloudThickness;
    if(h<=0||h>=1)return 0;
    const float3 p=worldPosition+float3(.72,0,.34)*(timeSeconds*cloudWindSpeed);
    const float3 q=p*candidateFrequency;
    const float wx=ValueNoise3D(q*.71+float3(11,3,17),cloudBaseNoiseSeed^0x51ed270bu)-.5;
    const float wz=ValueNoise3D(q*.71+float3(37,13,7),cloudDetailNoiseSeed^0xa511e9b3u)-.5;
    const float3 warped=q+float3(wx,0,wz)*candidateWarp;
    const float cell=ValueNoise3D(RotateNoise(warped,cloudBaseNoiseRotation,0),cloudBaseNoiseSeed);
    const float secondary=ValueNoise3D(RotateNoise(warped*1.67+float3(17,31,11),cloudDetailNoiseRotation,0),cloudDetailNoiseSeed);
    const float shape=smoothstep(candidateThreshold,candidateThreshold+.18,cell*.65+secondary*.35);
    const float detail=Fbm3D(p*.0047+29.3,cloudDetailNoiseSeed,cloudDetailNoiseRotation,3);
    // 上緣侵蝕較深，下緣較淺；核心不侵蝕，沒有位移整個 slab。
    const float bottomWidth=.10+candidateAsymmetry*(1-cell);
    const float topWidth=.16+candidateAsymmetry*secondary;
    const float profile=smoothstep(0,bottomWidth,h)*smoothstep(0,topWidth,1-h);
    const float edgeWeight=(1-shape)*(.35+.65*h);
    return max(0,shape-cloudErosion*detail*edgeWeight)*profile*cloudDensity*candidateMassScale;
}
float CandidateCloudShadowDensity(float3 p){return CandidateCloudDensity(p);}
