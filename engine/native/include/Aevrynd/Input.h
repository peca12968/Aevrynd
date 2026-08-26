#pragma once
#include <array>
#include <windows.h>
#include <Xinput.h>
#include "Math.h"

namespace Aevrynd {
class Input {
public:
    void BeginFrame(){}
    void ConsumePresses(){pressed_.fill(false);mousePressed_=false;gamepadPressed_=0;}
    void PollGamepad(){XINPUT_STATE state{};if(XInputGetState(0,&state)==ERROR_SUCCESS){gamepadConnected_=true;WORD buttons=state.Gamepad.wButtons;gamepadPressed_|=buttons&~gamepadButtons_;gamepadButtons_=buttons;auto axis=[](SHORT value,SHORT deadzone){if(value>deadzone)return float(value-deadzone)/float(32767-deadzone);if(value<-deadzone)return float(value+deadzone)/float(32768-deadzone);return 0.f;};gamepadLX_=axis(state.Gamepad.sThumbLX,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);gamepadLY_=axis(state.Gamepad.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);}else{gamepadConnected_=false;gamepadButtons_=0;gamepadLX_=gamepadLY_=0;}}
    void SetKey(unsigned key,bool down){ if(key<256){if(down&&!keys_[key])pressed_[key]=true;keys_[key]=down;} }
    bool Down(unsigned key)const{return key<256&&keys_[key];}
    bool Pressed(unsigned key)const{return key<256&&pressed_[key];}
    void SetMouse(Vec2 p){mouse_=p;}
    void SetMouseButton(bool down){if(down&&!mouseDown_)mousePressed_=true;mouseDown_=down;}
    Vec2 Mouse()const{return mouse_;}
    bool MousePressed()const{return mousePressed_;}
    bool GamepadConnected()const{return gamepadConnected_;}
    bool GamepadDown(WORD button)const{return(gamepadButtons_&button)!=0;}
    bool GamepadPressed(WORD button)const{return(gamepadPressed_&button)!=0;}
    float GamepadLeftX()const{return gamepadLX_;}float GamepadLeftY()const{return gamepadLY_;}
private:
    std::array<bool,256> keys_{}; std::array<bool,256> pressed_{};
    Vec2 mouse_{}; bool mouseDown_{},mousePressed_{};
    bool gamepadConnected_{};WORD gamepadButtons_{},gamepadPressed_{};float gamepadLX_{},gamepadLY_{};
};
}
