#pragma once

namespace Mistwatch {
inline void UpdatePlaybackControl(bool& started,bool& paused,bool focused,bool beginPressed,bool pausePressed) {
    // 切換視窗後保留暫停；只有玩家再次按下暫停鍵才恢復，不讓敵人趁背景操作攻擊。
    if(!focused){if(started)paused=true;return;}
    if(!started){if(beginPressed){started=true;paused=false;}return;}
    if(pausePressed)paused=!paused;
}
}
