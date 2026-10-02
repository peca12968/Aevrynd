#pragma once
#include <array>
#include <windows.h>
#include <Xinput.h>
#include "Math.h"

namespace Aevrynd {
class Input {
public:
    bool Focused()const{return focused_;}
    bool FocusLost()const{return focusLost_;}
    void SetFocused(bool focused){
        if(focused_==focused)return;
        const bool pendingLoss=focusLost_||!focused;
        focused_=focused;
        // 失焦可能收不到放開事件；清除持續輸入與邊緣，避免背景移動或恢復時鏡頭跳動。
        keys_.fill(false);ConsumePresses();mouseDown_=rightMouseDown_=false;
        focusLost_=pendingLoss;
        mouseInitialized_=false;mouseInsideCanvas_=false;
        gamepadConnected_=false;gamepadButtons_=0;gamepadLX_=gamepadLY_=gamepadRX_=gamepadRY_=0;
    }
    void BeginFrame(){}
    void ConsumePresses(bool preserveFocusLoss=false){pressed_.fill(false);mousePressed_=rightMousePressed_=false;mouseDelta_={};mouseWheel_=0;gamepadPressed_=0;if(!preserveFocusLoss)focusLost_=false;}
    void PollGamepad(){if(!focused_)return;XINPUT_STATE state{};if(XInputGetState(0,&state)==ERROR_SUCCESS){gamepadConnected_=true;WORD buttons=state.Gamepad.wButtons;gamepadPressed_|=buttons&~gamepadButtons_;gamepadButtons_=buttons;auto axis=[](SHORT value,SHORT deadzone){if(value>deadzone)return float(value-deadzone)/float(32767-deadzone);if(value<-deadzone)return float(value+deadzone)/float(32768-deadzone);return 0.f;};gamepadLX_=axis(state.Gamepad.sThumbLX,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);gamepadLY_=axis(state.Gamepad.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);gamepadRX_=axis(state.Gamepad.sThumbRX,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);gamepadRY_=axis(state.Gamepad.sThumbRY,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);}else{gamepadConnected_=false;gamepadButtons_=0;gamepadLX_=gamepadLY_=gamepadRX_=gamepadRY_=0;}}
    void SetKey(unsigned key,bool down){ if(focused_&&key<256){if(down&&!keys_[key])pressed_[key]=true;keys_[key]=down;} }
    bool Down(unsigned key)const{return key<256&&keys_[key];}
    bool Pressed(unsigned key)const{return key<256&&pressed_[key];}
    void SetMouse(Vec2 p,bool insideCanvas=true){if(!focused_)return;if(mouseInitialized_&&mouseInsideCanvas_&&insideCanvas)mouseDelta_=mouseDelta_+(p-mouse_);mouse_=p;mouseInitialized_=true;mouseInsideCanvas_=insideCanvas;}
    void AddMouseWheel(float delta){if(focused_)mouseWheel_+=delta;}
    void SetMouseButton(bool down){if(!focused_)return;if(down&&!mouseDown_)mousePressed_=true;mouseDown_=down;}
    void SetRightMouseButton(bool down){if(!focused_)return;if(down&&!rightMouseDown_)rightMousePressed_=true;rightMouseDown_=down;}
    Vec2 Mouse()const{return mouse_;}
    Vec2 MouseDelta()const{return mouseDelta_;}
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
    bool focused_{true},focusLost_{};
    std::array<bool,256> keys_{}; std::array<bool,256> pressed_{};
    Vec2 mouse_{},mouseDelta_{};float mouseWheel_{};bool mouseInitialized_{},mouseInsideCanvas_{true},mouseDown_{},mousePressed_{},rightMouseDown_{},rightMousePressed_{};
    bool gamepadConnected_{};WORD gamepadButtons_{},gamepadPressed_{};float gamepadLX_{},gamepadLY_{},gamepadRX_{},gamepadRY_{};
};
}
