#pragma once
#include <array>
#include <chrono>
#include <optional>
#include <windows.h>
#include <Xinput.h>
#include "Math.h"

namespace Aevrynd {
class Input {
public:
    using Clock=std::chrono::steady_clock;
    bool Focused()const{return focused_;}
    // 保留第一次中斷及其時間，直到下一次固定更新取走輸入。
    std::optional<Clock::time_point> SuspendedAt()const noexcept{return suspendedAt_;}
    void MarkSuspended(Clock::time_point now=Clock::now()) noexcept {
        if(!suspendedAt_)suspendedAt_=now;
        focusLost_=true;
        ResetDeviceState();
    }
    bool FocusLost()const{return focusLost_;}
    void SetFocused(bool focused){
        if(focused_==focused)return;
        focused_=focused;
        if(focused_)ResetDeviceState();
        else MarkSuspended();
    }
    void BeginFrame(){}
    void ConsumePresses(bool preserveFocusLoss=false){pressed_.fill(false);mousePressed_=rightMousePressed_=false;mouseDelta_={};leftMouseDragDelta_={};mouseWheel_=0;gamepadPressed_=0;if(!preserveFocusLoss){focusLost_=false;suspendedAt_.reset();}}
    void PollGamepad(){if(!focused_)return;XINPUT_STATE state{};if(XInputGetState(0,&state)==ERROR_SUCCESS){gamepadConnected_=true;WORD buttons=state.Gamepad.wButtons;gamepadPressed_|=buttons&~gamepadButtons_;gamepadButtons_=buttons;auto axis=[](SHORT value,SHORT deadzone){if(value>deadzone)return float(value-deadzone)/float(32767-deadzone);if(value<-deadzone)return float(value+deadzone)/float(32768-deadzone);return 0.f;};gamepadLX_=axis(state.Gamepad.sThumbLX,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);gamepadLY_=axis(state.Gamepad.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);gamepadRX_=axis(state.Gamepad.sThumbRX,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);gamepadRY_=axis(state.Gamepad.sThumbRY,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);}else{gamepadConnected_=false;gamepadButtons_=0;gamepadLX_=gamepadLY_=gamepadRX_=gamepadRY_=0;}}
    void SetKey(unsigned key,bool down){ if(focused_&&key<256){if(down&&!keys_[key])pressed_[key]=true;keys_[key]=down;} }
    bool Down(unsigned key)const{return key<256&&keys_[key];}
    bool Pressed(unsigned key)const{return key<256&&pressed_[key];}
    void SetMouse(Vec2 p,bool insideCanvas=true){
        if(!focused_)return;
        if(mouseInitialized_&&mouseInsideCanvas_&&insideCanvas){
            const Vec2 delta=p-mouse_;mouseDelta_=mouseDelta_+delta;
            // 依事件當時的按鍵狀態分類，不能用整批最後的MouseDown回推按下前的位移。
            if(mouseDown_)leftMouseDragDelta_=leftMouseDragDelta_+delta;
        }
        mouse_=p;mouseInitialized_=true;mouseInsideCanvas_=insideCanvas;
    }
    void AddMouseWheel(float delta){if(focused_)mouseWheel_+=delta;}
    // 擷取遭取消時只丟棄尚未提交的左鍵拖曳；不影響鍵盤、右鍵、滾輪或焦點。
    // 正常放開已先記錄末段位移，無 active held 時必須保留該段，不能事後抹除。
    void CancelLeftMouseDrag() noexcept {
        if(!mouseDown_)return;
        mouseDown_=false;mousePressed_=false;leftMouseDragDelta_={};
        mouseInitialized_=false;
    }
    void SetMouseButton(bool down){if(!focused_)return;if(down&&!mouseDown_)mousePressed_=true;mouseDown_=down;}
    void SetMouseButtonAt(Vec2 p,bool down,bool insideCanvas=true){
        // 按鍵訊息也帶座標：先按舊held狀態記錄至邊界，再切換按鍵，避免漏掉最後一段。
        SetMouse(p,insideCanvas);
        if(!down||insideCanvas)SetMouseButton(down);
    }
    void SetRightMouseButton(bool down){if(!focused_)return;if(down&&!rightMouseDown_)rightMousePressed_=true;rightMouseDown_=down;}
    Vec2 Mouse()const{return mouse_;}
    Vec2 MouseDelta()const{return mouseDelta_;}
    // 保留放開前已完成的合法拖曳；不包含按下前、放開後或跨畫布邊界的位移。
    Vec2 LeftMouseDragDelta()const{return leftMouseDragDelta_;}
    float MouseWheel()const{return mouseWheel_;}
    bool MousePressed()const{return mousePressed_;}
    bool MouseDown()const{return mouseDown_;}
    bool RightMousePressed()const{return rightMousePressed_;}
    bool MouseInsideCanvas()const{return mouseInsideCanvas_;}
    bool GamepadConnected()const{return gamepadConnected_;}
    bool GamepadDown(WORD button)const{return(gamepadButtons_&button)!=0;}
    bool GamepadPressed(WORD button)const{return(gamepadPressed_&button)!=0;}
    float GamepadLeftX()const{return gamepadLX_;}float GamepadLeftY()const{return gamepadLY_;}
    float GamepadRightX()const{return gamepadRX_;}float GamepadRightY()const{return gamepadRY_;}
private:
    void ResetDeviceState() noexcept {
        // 最小化或失焦可能收不到放開事件，避免舊輸入跨過中斷。
        keys_.fill(false);ConsumePresses(true);mouseDown_=rightMouseDown_=false;
        mouseInitialized_=false;mouseInsideCanvas_=false;
        gamepadConnected_=false;gamepadButtons_=0;gamepadLX_=gamepadLY_=gamepadRX_=gamepadRY_=0;
    }
    bool focused_{true},focusLost_{};
    std::optional<Clock::time_point> suspendedAt_;
    std::array<bool,256> keys_{}; std::array<bool,256> pressed_{};
    Vec2 mouse_{},mouseDelta_{},leftMouseDragDelta_{};float mouseWheel_{};bool mouseInitialized_{},mouseInsideCanvas_{true},mouseDown_{},mousePressed_{},rightMouseDown_{},rightMousePressed_{};
    bool gamepadConnected_{};WORD gamepadButtons_{},gamepadPressed_{};float gamepadLX_{},gamepadLY_{},gamepadRX_{},gamepadRY_{};
};
}
