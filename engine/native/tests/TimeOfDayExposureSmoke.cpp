#include <Aevrynd/Renderer.h>
#include <cmath>
#include <iostream>
#include <limits>
using namespace Aevrynd;
int main(){TimeOfDayExposureSettings3D hostile{};hostile.dayExposureEv=std::numeric_limits<float>::quiet_NaN();hostile.duskExposureEv=-9;hostile.nightExposureEv=-8;hostile.shadowLift=9;hostile.toeStrength=-1;hostile.shoulderStrength=99;const auto normalized=NormalizeTimeOfDayExposureSettings3D(hostile);if(normalized.dayExposureEv!=.2f||normalized.duskExposureEv!=.2f||normalized.nightExposureEv!=.2f||normalized.shadowLift!=.08f||normalized.toeStrength!=0||normalized.shoulderStrength!=2||!TimeOfDayExposureFingerprint3D(normalized)){std::cerr<<"Time-of-day hostile normalization failed\n";return 1;}DrawList draw;draw.SetTimeOfDayExposure(normalized);if(draw.TimeOfDayExposure()!=normalized)return 2;const auto disabled=NormalizeTimeOfDayExposureSettings3D({false});if(disabled.enabled||!TimeOfDayExposureFingerprint3D(disabled))return 3;std::cout<<"fingerprint="<<TimeOfDayExposureFingerprint3D(normalized)<<'\n';}
