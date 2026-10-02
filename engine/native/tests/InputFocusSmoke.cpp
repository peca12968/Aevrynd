#include <Aevrynd/Engine.h>
#include <iostream>
#include <memory>
#include <thread>

using namespace Aevrynd;
class FocusProbe final : public IGame {
public:
    unsigned& failures;
    explicit FocusProbe(unsigned& result):failures(result){}
    std::thread restoreWindow;
    bool observedRestore{};
    void OnStart(Engine& engine) override {
        auto& input=engine.GetInput();
        SendMessageW(engine.Window(),WM_SETFOCUS,0,0);
        SendMessageW(engine.Window(),WM_KEYDOWN,'W',0);
        input.SetMouse({10,10});input.SetMouse({30,20});input.AddMouseWheel(2);
        input.SetMouseButton(true);input.SetRightMouseButton(true);
        if(!input.Down('W')||!input.Pressed('W')||!input.MouseDown())++failures;
        SendMessageW(engine.Window(),WM_KILLFOCUS,0,0);
        SendMessageW(engine.Window(),WM_KEYDOWN,'W',0);
        input.SetMouseButton(true);input.SetRightMouseButton(true);input.AddMouseWheel(3);
        input.PollGamepad();
        if(input.Down('W')||input.Pressed('W')||input.MouseDown()||input.MousePressed()||
            input.RightMousePressed()||input.MouseWheel()!=0||Length(input.MouseDelta())!=0) {
            std::cerr<<"FIRST_DIVERGENCE: focus loss retains gameplay input\n";++failures;
        }
        if(input.Focused()||!input.FocusLost()||input.GamepadConnected()||input.GamepadLeftX()!=0)++failures;
        SendMessageW(engine.Window(),WM_SETFOCUS,0,0);
        if(!input.Focused()||!input.FocusLost())++failures;
        input.SetMouse({600,400});
        if(Length(input.MouseDelta())!=0||input.Down('W')||input.Pressed('W'))++failures;
        SendMessageW(engine.Window(),WM_KEYDOWN,'W',0);
        if(!input.Down('W')||!input.Pressed('W'))++failures;
        input.SetKey('W',false);input.ConsumePresses();
        if(input.FocusLost())++failures;
        // 在第一個固定更新前最小化；必須經過引擎背景迴圈，不能先由遊戲消費失焦。
        SendMessageW(engine.Window(),WM_KILLFOCUS,0,0);
        SendMessageW(engine.Window(),WM_SIZE,SIZE_MINIMIZED,0);
        restoreWindow=std::thread([window=engine.Window()] {
            Sleep(200);
            PostMessageW(window,WM_SIZE,SIZE_RESTORED,MAKELPARAM(1280,720));
            PostMessageW(window,WM_SETFOCUS,0,0);
        });
    }
    void OnUpdate(Engine& engine,float) override {
        if(observedRestore)return;
        observedRestore=true;
        // 首次更新前只有最小化分支會修剪；若恢復訊息提早到達而漏測背景迴圈，必須失敗。
        const auto minimizedTrimCount=engine.GetMemoryStats().trimCount;
        if(minimizedTrimCount==0){
            std::cerr<<"FIRST_DIVERGENCE: minimized engine branch was not observed\n";++failures;
        }
        if(!engine.GetInput().Focused()||!engine.GetInput().FocusLost()){
            std::cerr<<"FIRST_DIVERGENCE: minimized loop erased unconsumed focus loss\n";++failures;
        }
        std::cout<<"input_focus_failures="<<failures<<" minimized_trim_count="<<minimizedTrimCount<<'\n';engine.Quit();
    }
    void OnRender(Engine&,float) override {}
    void OnStop(Engine&) override {if(restoreWindow.joinable())restoreWindow.join();}
};
int main() {
    unsigned failures{};
    Engine engine;
    const int exit=engine.Run(std::make_unique<FocusProbe>(failures),GetModuleHandleW(nullptr),SW_HIDE);
    return exit?exit:failures?1:0;
}
