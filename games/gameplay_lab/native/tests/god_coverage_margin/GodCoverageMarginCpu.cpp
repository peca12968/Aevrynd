#include "GameplayLabGame.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace GameplayLab;
void Require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){try{
 static_assert(GameplayGodMinimumPlayerCoverage3D==.0004f);
 static_assert(GameplayGodMinimumPlayerPixels3D==369);
 static_assert(GameplayGodCandidatePlayerPixels3D==462);
 const float old=24.f/(320.f*180.f),next=29.f/(320.f*180.f);
 Require(old>=GameplayGodMinimumPlayerCoverage3D,"old nominal accepted");
 Require(!GameplayGodCandidatePlayerCoveragePass3D(old),"old 384 equivalent pixels must reject");
 Require(!GameplayGodCandidatePlayerCoveragePass3D(28.f/(320.f*180.f)),"28 coarse samples must reject");
 Require(GameplayGodCandidatePlayerCoveragePass3D(next),"29 coarse samples must pass coverage only");
 Require(!GameplayGodCandidatePlayerCoveragePass3D(std::numeric_limits<float>::quiet_NaN()),"NaN");
 Require(!GameplayGodCandidatePlayerCoveragePass3D(std::numeric_limits<float>::infinity()),"infinity");
 Require(!GameplayGodCandidatePlayerCoveragePass3D(-1.f),"negative");
 Require(!GameplayGodCandidatePlayerCoveragePass3D(1.1f),"above image");
 Require(464-58-16>=GameplayGodMinimumPlayerPixels3D,"observed deficit plus one coarse sample");
 GameplayCaptureProjectionEvidence3D formal{Aevrynd::ProjectionMode::Orthographic,0,26,old,.1f,.04f,.02f,20,1};
 Require(formal.Complete(GameplayCaptureCamera3D::God),"formal gate remains unchanged");
 formal.godActualTriangleCoverage=.00039f;Require(!formal.Complete(GameplayCaptureCamera3D::God),"formal below gate rejects");
 if(argc!=2)return 2;std::ofstream out(argv[1]);out<<"{\"formalMinimumPixels\":369,\"formalMinimumCoverage\":0.0004,\"oldCoarseSamples\":24,\"oldEquivalentPixels\":384,\"observedOidPixels\":326,\"observedLossPixels\":58,\"coarseQuantumPixels\":16,\"candidateFactor\":1.25,\"candidateMinimumEquivalentPixels\":462,\"minimumCoarseSamples\":29,\"minimumCoarseEquivalentPixels\":464,\"gpuExecuted\":false,\"scope\":\"candidate margin only; not a universal occlusion bound or GPU acceptance\"}";
 std::cout<<"PASS old candidate rejected; formal369 unchanged; candidate462/coarse464; hostile inputs rejected\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
