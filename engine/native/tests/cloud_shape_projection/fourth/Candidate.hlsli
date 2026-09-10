// 獨立研究候選，未接入正式 shader。低頻形體先成形，高頻只侵蝕邊緣。
float candidateFrequency=.00022;
float candidateThreshold=.50;
float candidateMassScale=1.0;
float CandidateCloudDensity(float3 worldPosition)
{
    if(!isfinite(worldPosition.x)||!isfinite(worldPosition.y)||!isfinite(worldPosition.z)||
       !isfinite(cloudThickness)||!isfinite(cloudDensity)||!isfinite(timeSeconds)||
       !isfinite(candidateMassScale)||!isfinite(candidateFrequency)||!isfinite(candidateThreshold)||
       cloudThickness<=0.0||cloudDensity<=0.0||candidateMassScale<=0.0||candidateFrequency<=0.0)return 0.0;
    const float h=(worldPosition.y-cloudBaseAltitude)/cloudThickness;
    if(h<=0.0||h>=1.0)return 0.0;
    const float3 advected=worldPosition+float3(.72,0,.34)*(timeSeconds*cloudWindSpeed);
    const float3 coordinate=advected*candidateFrequency;
    const float lowA=ValueNoise3D(RotateNoise(coordinate,cloudBaseNoiseRotation,0),cloudBaseNoiseSeed);
    const float lowB=ValueNoise3D(RotateNoise(coordinate*1.67+float3(17,31,11),cloudDetailNoiseRotation,0),cloudDetailNoiseSeed);
    const float shape=smoothstep(candidateThreshold,candidateThreshold+.18,lowA*.65+lowB*.35);
    const float detail=Fbm3D(advected*.0047+29.3,cloudDetailNoiseSeed,cloudDetailNoiseRotation,3);
    const float erosion=cloudErosion*detail*(1.0-shape)*.65;
    const float profile=smoothstep(0.0,.16,h)*smoothstep(0.0,.28,1.0-h);
    return max(0.0,shape-erosion)*profile*cloudDensity*candidateMassScale;
}
float CandidateCloudShadowDensity(float3 worldPosition)
{
    // 陰影與主路徑共享完全相同的密度，不使用不同的簡化包絡。
    return CandidateCloudDensity(worldPosition);
}
