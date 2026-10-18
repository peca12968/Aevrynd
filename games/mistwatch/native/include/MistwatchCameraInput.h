#pragma once
#include <Aevrynd/Input.h>
#include "MistwatchCamera.h"

namespace Mistwatch {
// 輸入接線與鏡頭數學分離，避免將Win32 Input型別帶入所有鏡頭／世界規則使用者。
inline void UpdateOrbitFromInput(OrbitCamera& orbit,const Aevrynd::Input& input,float keyboard,float dt){
    // 放開按鍵前的最後一段仍有效；Input已按每個事件過濾焦點與畫布邊界。
    orbit.Update(input.LeftMouseDragDelta(),input.MouseWheel(),keyboard,dt);
}
}
