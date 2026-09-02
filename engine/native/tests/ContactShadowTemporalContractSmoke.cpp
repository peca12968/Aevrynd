#include <Aevrynd/Renderer.h>
#include <cmath>
#include <iostream>
#include <limits>
using namespace Aevrynd;
int main(){
 ContactShadowTemporalConstants3D c{};c.historyValid=1;c.historyWeight=.75f;c.depthThreshold=.02f;c.motionThresholdPixels=4.f;
 const auto stable=EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.505f,1.f,true},c);
 const auto raw=EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.505f,1.f,true},c,false);
 const auto depth=EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.8f,1.f,true},c);
 const auto motion=EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.505f,8.f,true},c);
 auto cutConstants=c;cutConstants.cameraCut=1;const auto cut=EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.505f,1.f,true},cutConstants);
 auto resizeConstants=c;resizeConstants.resized=1;const auto resize=EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.505f,1.f,true},resizeConstants);
 auto qualityConstants=c;qualityConstants.qualityChanged=1;const auto quality=EvaluateContactShadowTemporal3D({.2f,.8f,.5f,.505f,1.f,true},qualityConstants);
 if(!stable.historyAccepted||std::abs(stable.visibility-.65f)>1e-6f||raw.historyAccepted||raw.rejection!=ContactShadowTemporalReset3D::Disabled||depth.rejection!=ContactShadowTemporalReset3D::DepthRejected||motion.rejection!=ContactShadowTemporalReset3D::MotionRejected||cut.rejection!=ContactShadowTemporalReset3D::CameraCut||resize.rejection!=ContactShadowTemporalReset3D::Resize||quality.rejection!=ContactShadowTemporalReset3D::Quality){std::cerr<<"contact shadow temporal contract failed\n";return 1;}
 const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
 for(const float nonfinite:{nan,inf,-inf}){const auto current=EvaluateContactShadowTemporal3D({nonfinite,.8f,.5f,.5f,0.f,true},c),history=EvaluateContactShadowTemporal3D({.4f,nonfinite,.5f,.5f,0.f,true},c);if(current.historyAccepted||current.rejection!=ContactShadowTemporalReset3D::ReprojectionInvalid||!std::isfinite(current.visibility)||std::abs(current.visibility-1.f)>1e-6f||history.historyAccepted||history.rejection!=ContactShadowTemporalReset3D::ReprojectionInvalid||std::abs(history.visibility-.4f)>1e-6f){std::cerr<<"nonfinite visibility parity failed\n";return 3;}}
 // 負向控制：未濾波序列的 frame-to-frame 差異必須顯著高於 temporal 結果。
 const auto a=EvaluateContactShadowTemporal3D({.1f,.5f,.5f,.5f,0.f,true},c),b=EvaluateContactShadowTemporal3D({.9f,a.visibility,.5f,.5f,0.f,true},c);if(std::abs(b.visibility-a.visibility)>=.8f){std::cerr<<"temporal stabilization negative control failed\n";return 2;}return 0;
}
