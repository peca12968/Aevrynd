#pragma once
#include <array>
#include <windows.h>
#include <Xinput.h>
#include "Math.h"

namespace Aevrynd {
class Input {
public:
    void BeginFrame(){}
    void ConsumePresses(){pressed_.fill(false);mousePressed_=false;mouseDelta_={};mouseWheel_=0;gamepadPressed_=0;}
    void PollGamepad(){XINPUT_STATE state{};if(XInputGetState(0,&state)==ERROR_SUCCESS){gamepadConnected_=true;WORD buttons=state.Gamepad.wButtons;gamepadPressed_|=buttons&~gamepadButtons_;gamepadButtons_=buttons;auto axis=[](SHORT value,SHORT deadzone){if(value>deadzone)return float(value-deadzone)/float(32767-deadzone);if(value<-deadzone)return float(value+deadzone)/float(32768-deadzone);return 0.f;};gamepadLX_=axis(state.Gamepad.sThumbLX,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);gamepadLY_=axis(state.Gamepad.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);gamepadRX_=axis(state.Gamepad.sThumbRX,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);gamepadRY_=axis(state.Gamepad.sThumbRY,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);}else{gamepadConnected_=false;gamepadButtons_=0;gamepadLX_=gamepadLY_=gamepadRX_=gamepadRY_=0;}}
    void SetKey(unsigned key,bool down){ if(key<256){if(down&&!keys_[key])pressed_[key]=true;keys_[key]=down;} }
    bool Down(unsigned key)const{return key<256&&keys_[key];}
    bool Pressed(unsigned key)const{return key<256&&pressed_[key];}
    void SetMouse(Vec2 p,bool insideCanvas=true){if(mouseInitialized_&&mouseInsideCanvas_&&insideCanvas)mouseDelta_=mouseDelta_+(p-mouse_);mouse_=p;mouseInitialized_=true;mouseInsideCanvas_=insideCanvas;}
    void AddMouseWheel(float delta){mouseWheel_+=delta;}
    void SetMouseButton(bool down){if(down&&!mouseDown_)mousePressed_=true;mouseDown_=down;}
    Vec2 Mouse()const{return mouse_;}
    Vec2 MouseDelta()const{return mouseDelta_;}
    float MouseWheel()const{return mouseWheel_;}
    bool MousePressed()const{return mousePressed_;}
    bool MouseInsideCanvas()const{return mouseInsideCanvas_;}
    bool GamepadConnected()const{return gamepadConnected_;}
    bool GamepadDown(WORD button)const{return(gamepadButtons_&button)!=0;}
    bool GamepadPressed(WORD button)const{return(gamepadPressed_&button)!=0;}
    float GamepadLeftX()const{return gamepadLX_;}float GamepadLeftY()const{return gamepadLY_;}
    float GamepadRightX()const{return gamepadRX_;}float GamepadRightY()const{return gamepadRY_;}
private:
    std::array<bool,256> keys_{}; std::array<bool,256> pressed_{};
    Vec2 mouse_{},mouseDelta_{};float mouseWheel_{};bool mouseInitialized_{},mouseInsideCanvas_{true},mouseDown_{},mousePressed_{};
    bool gamepadConnected_{};WORD gamepadButtons_{},gamepadPressed_{};float gamepadLX_{},gamepadLY_{},gamepadRX_{},gamepadRY_{};
};
}
