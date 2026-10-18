#include <Aevrynd/Engine.h>
#include <Aevrynd/Viewport.h>
#include <windowsx.h>
#include <initializer_list>
#include <limits>
#include <iostream>
#include <memory>
#include <thread>
#include <stdexcept>

using namespace Aevrynd;
class FocusProbe final : public IGame {
public:
    unsigned& failures;
    explicit FocusProbe(unsigned& result):failures(result){}
    std::thread restoreWindow;
    bool observedRestore{};

    static LRESULT CALLBACK CaptureWindowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
        // 輔助視窗收到放開不自動交還擷取，才能驗證遊戲沒有誤釋放別人的擷取。
        if(message==WM_LBUTTONUP)return 0;
        return DefWindowProcW(window,message,wParam,lParam);
    }
    unsigned captureCases{},captureAssertions{};
    void CaptureCancellationContract(Engine& engine) {
        auto& input=engine.GetInput();const HWND game=engine.Window();
        std::string active="create-helper";HWND helper{};ATOM helperClass{};
        struct Cleanup {
            HWND& helper;ATOM& helperClass;HWND game;
            ~Cleanup(){
                const HWND capture=GetCapture();
                if(capture==game||(helper&&capture==helper))ReleaseCapture();
                if(helper)DestroyWindow(helper);
                if(helperClass)UnregisterClassW(MAKEINTATOM(helperClass),GetModuleHandleW(nullptr));
            }
        } cleanup{helper,helperClass,game};
        try{
            const auto require=[&](bool value,const char* reason){++captureAssertions;if(!value)throw std::runtime_error(reason);};
            const auto same=[](Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;};
            const DWORD thread=GetCurrentThreadId();const HWND focus=GetFocus();
            WNDCLASSW helperDescription{};helperDescription.lpfnWndProc=CaptureWindowProc;
            helperDescription.hInstance=GetModuleHandleW(nullptr);helperDescription.lpszClassName=L"AevryndCaptureCancellationContract";
            helperClass=RegisterClassW(&helperDescription);
            require(helperClass!=0,"helper class registration failed");
            helper=CreateWindowExW(WS_EX_NOACTIVATE,MAKEINTATOM(helperClass),L"Aevrynd capture contract",WS_POPUP,
                0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            require(helper!=nullptr,"helper HWND creation failed");
            require(GetWindowThreadProcessId(helper,nullptr)==thread&&GetWindowThreadProcessId(game,nullptr)==thread,"capture HWNDs are not on the test UI thread");
            const auto preserve=[&]{
                require(input.Down('W')&&input.Pressed('W')&&!input.Down('E')&&input.Pressed('E'),"capture cancellation changed keyboard");
                require(input.Focused()&&!input.FocusLost()&&GetFocus()==focus,"capture contract changed focus");
                require(input.RightMousePressed()&&input.MouseWheel()==2,"capture cancellation changed right button or wheel");
            };
            const auto prepare=[&]{
                if(GetCapture()==helper)ReleaseCapture();
                input.SetMouseButton(false);input.ConsumePresses();
                input.SetKey('W',false);input.SetKey('W',true);input.SetKey('E',true);input.SetKey('E',false);
                input.SetRightMouseButton(false);input.SetRightMouseButton(true);input.AddMouseWheel(2);
                SendMessageW(game,WM_MOUSEMOVE,0,MAKELPARAM(100,100));
                SendMessageW(game,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(100,100));
                SendMessageW(game,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(110,115));
                require(GetCapture()==game,"game did not acquire real OS capture");
                require(input.MouseDown()&&input.MousePressed()&&Length(input.LeftMouseDragDelta())>0,"active drag fixture missing");
                preserve();
            };
            active="same-ui-thread-capture-theft";prepare();
            const Vec2 raw=input.MouseDelta();
            SetCapture(helper);
            require(GetCapture()==helper,"helper did not acquire real OS capture");
            preserve();
            require(!input.MouseDown()&&!input.MousePressed()&&same(input.LeftMouseDragDelta(),{}),"real WM_CAPTURECHANGED retained active left drag");
            require(same(input.MouseDelta(),raw),"capture theft changed accumulated raw delta");
            // 放開只送給現在的擷取擁有者，不能向遊戲補發 Up 掩蓋舊來源的錯誤。
            SendMessageW(helper,WM_LBUTTONUP,0,MAKELPARAM(500,500));
            SendMessageW(game,WM_MOUSEMOVE,0,MAKELPARAM(500,500));
            require(same(input.MouseDelta(),raw),"first post-theft point jumped");
            SendMessageW(game,WM_MOUSEMOVE,0,MAKELPARAM(510,505));
            require(!input.MouseDown()&&same(input.LeftMouseDragDelta(),{}),"unpressed move after capture theft became orbit drag");
            require(GetCapture()==helper,"game released another HWND capture");preserve();++captureCases;

            active="cancel-mode-own-capture";prepare();
            SendMessageW(game,WM_CANCELMODE,0,0);
            require(GetCapture()==nullptr,"cancel-mode did not release own capture");
            require(!input.MouseDown()&&!input.MousePressed()&&same(input.LeftMouseDragDelta(),{}),"cancel-mode retained active left drag");
            preserve();
            SendMessageW(game,WM_CANCELMODE,0,0);
            require(GetCapture()==nullptr&&!input.MouseDown(),"repeated cancel-mode changed capture state");
            preserve();++captureCases;

            active="cancel-mode-foreign-capture";prepare();SetCapture(helper);
            require(GetCapture()==helper,"foreign capture fixture missing");
            // 直接設一個尚未取消的 left held，確認 cancel 不會藉此釋放他人的 capture。
            input.SetMouseButton(true);require(input.MouseDown(),"foreign-owner active-left fixture missing");
            SendMessageW(game,WM_CANCELMODE,0,0);
            require(GetCapture()==helper,"cancel-mode released another HWND capture");
            require(!input.MouseDown()&&!input.MousePressed()&&same(input.LeftMouseDragDelta(),{}),"foreign-owner cancel retained left held");
            preserve();++captureCases;

            active="normal-up-preserves-final-drag";prepare();
            const Vec2 before=input.LeftMouseDragDelta(),anchor=input.Mouse();
            SendMessageW(game,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(120,130));
            const Vec2 moved=input.Mouse();
            SendMessageW(game,WM_LBUTTONUP,0,MAKELPARAM(130,140));
            const Vec2 tail=input.Mouse(),expected=before+(moved-anchor)+(tail-moved);
            require(GetCapture()==nullptr&&!input.MouseDown()&&input.MousePressed(),"normal-up capture release or edge differs");
            require(same(input.LeftMouseDragDelta(),expected)&&Length(expected)>0,"normal-up self capturechanged erased valid final drag");
            // 額外重送取消通知也不得抹掉已完成拖曳；這不是 theft 的真消息證據。
            SendMessageW(game,WM_CAPTURECHANGED,0,reinterpret_cast<LPARAM>(helper));
            require(same(input.LeftMouseDragDelta(),expected)&&input.MousePressed(),"duplicate capturechanged erased completed drag");
            preserve();++captureCases;
            active="normal-up-foreign-capture";prepare();SetCapture(helper);
            require(GetCapture()==helper,"foreign normal-up fixture missing");
            SendMessageW(game,WM_LBUTTONUP,0,MAKELPARAM(130,140));
            require(GetCapture()==helper,"normal-up released another HWND capture");
            require(!input.MouseDown()&&!input.MousePressed()&&same(input.LeftMouseDragDelta(),{}),"foreign normal-up restored cancelled left drag");
            preserve();++captureCases;
            require(captureCases==5,"capture integration case accounting differs");
            std::cout<<"INPUT_CAPTURE_CANCELLATION_INTEGRATION_PASS cases="<<captureCases
                <<" assertions="<<captureAssertions<<" actual_os_capture=true same_ui_thread=true engine_gpu_initialized=true\n";
        }catch(const std::exception& error){
            std::cerr<<"INPUT_CAPTURE_CANCELLATION_FIRST_DIVERGENCE case="<<active<<" assertions="<<captureAssertions
                <<" reason="<<error.what()<<'\n';++failures;
        }
    }

    void WheelCanvasContract(Engine& engine) {
        auto& input=engine.GetInput();const HWND game=engine.Window();
        const Input priorInput=input;RECT original{};const HWND priorFocus=GetFocus(),priorCapture=GetCapture(),priorForeground=GetForegroundWindow();
        const DWORD thread=GetCurrentThreadId();unsigned wheelCases{},wheelAssertions{},wheelFailures{};
        bool haveOriginal{},restored{};std::string active="setup";
        const auto require=[&](bool ok,const char* reason){++wheelAssertions;if(!ok)throw std::runtime_error(reason);};
        const auto report=[&](const std::exception& error){++wheelFailures;++failures;
            std::cerr<<"INPUT_WHEEL_CANVAS_FIRST_DIVERGENCE case="<<active<<" reason="<<error.what()<<'\n';};
        const auto same=[](Vec2 a,Vec2 b){return a.x==b.x&&a.y==b.y;};
        const auto restore=[&]{
            if(haveOriginal){
                require(SetWindowPos(game,nullptr,original.left,original.top,original.right-original.left,original.bottom-original.top,
                    SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE,"restore own rectangle failed");
                RECT actual{};require(GetWindowRect(game,&actual)!=FALSE&&actual.left==original.left&&actual.top==original.top&&
                    actual.right==original.right&&actual.bottom==original.bottom,"restored rectangle differs");
            }
            input=priorInput;
            require(GetFocus()==priorFocus&&GetCapture()==priorCapture&&GetForegroundWindow()==priorForeground,"wheel test changed OS focus/capture/foreground");
            require(!IsWindowVisible(game),"wheel test exposed hidden window");restored=true;
        };
        try{
            DWORD process{};const auto ownerThread=GetWindowThreadProcessId(game,&process);
            require(ownerThread==thread&&process==GetCurrentProcessId(),"wheel HWND not owned by test UI thread");
            require(!IsWindowVisible(game),"wheel test requires hidden owned window");
            require(GetWindowRect(game,&original)!=FALSE,"original rectangle unavailable");haveOriginal=true;
            const auto resize=[&](LONG width,LONG height,LONG x,LONG y){
                RECT outer{},client{};require(GetWindowRect(game,&outer)&&GetClientRect(game,&client),"resize source rectangles unavailable");
                const LONG outerWidth=outer.right-outer.left+width-(client.right-client.left),
                    outerHeight=outer.bottom-outer.top+height-(client.bottom-client.top);
                require(SetWindowPos(game,nullptr,x,y,outerWidth,outerHeight,SWP_NOACTIVATE|SWP_NOZORDER)!=FALSE,"resize own hidden window failed");
                require(GetClientRect(game,&client)&&client.right-client.left==width&&client.bottom-client.top==height,"actual client size differs");
                require(!IsWindowVisible(game)&&GetFocus()==priorFocus&&GetCapture()==priorCapture&&GetForegroundWindow()==priorForeground,"resize activated or exposed window");
            };
            struct Event {LONG x,y;SHORT delta;float expected;bool inside;};
            const auto run=[&](const char* name,LONG width,LONG height,bool staleInside,bool focused,bool negative,
                std::initializer_list<Event> events){
                active=name;++wheelCases;
                try{
                    SendMessageW(game,WM_SETFOCUS,0,0);input.SetMouseButton(false);input.SetRightMouseButton(false);input.ConsumePresses();
                    input.SetKey('W',false);input.SetKey('W',true);input.SetKey('E',true);input.SetKey('E',false);
                    input.SetMouse({17,19},staleInside);input.SetMouseButton(true);input.SetMouse({23,29},staleInside);input.SetRightMouseButton(true);
                    if(!focused){SendMessageW(game,WM_KILLFOCUS,0,0);input.ConsumePresses();}
                    const auto mouse=input.Mouse(),delta=input.MouseDelta(),drag=input.LeftMouseDragDelta();
                    const bool left=input.MouseDown(),leftPressed=input.MousePressed(),rightPressed=input.RightMousePressed(),
                        insideBefore=input.MouseInsideCanvas(),focusedBefore=input.Focused(),lostBefore=input.FocusLost();
                    const bool heldW=input.Down('W'),edgeW=input.Pressed('W'),heldE=input.Down('E'),edgeE=input.Pressed('E');
                    RECT client{};require(GetClientRect(game,&client)&&client.right-client.left==width&&client.bottom-client.top==height,"case actual client changed");
                    const auto viewport=CalculateCanvasViewport({1280,720},{float(width),float(height)});
                    for(const auto& event:events){
                        require(viewport.Contains({float(event.x),float(event.y)})==event.inside,"event viewport classification differs");
                        POINT screen{event.x,event.y};require(ClientToScreen(game,&screen)!=FALSE,"wheel ClientToScreen failed");
                        require(screen.x>=(std::numeric_limits<SHORT>::min)()&&screen.x<=(std::numeric_limits<SHORT>::max)()&&
                            screen.y>=(std::numeric_limits<SHORT>::min)()&&screen.y<=(std::numeric_limits<SHORT>::max)(),"screen point cannot be encoded signed");
                        if(negative)require(screen.x<0&&screen.y<0,"negative-screen fixture not actually negative");
                        const LPARAM position=MAKELPARAM(static_cast<SHORT>(screen.x),static_cast<SHORT>(screen.y));
                        require(GET_X_LPARAM(position)==screen.x&&GET_Y_LPARAM(position)==screen.y,"signed wheel coordinates lost");
                        SendMessageW(game,WM_MOUSEWHEEL,MAKEWPARAM(0,static_cast<WORD>(event.delta)),position);
                        require(input.MouseWheel()==event.expected,"wheel delta accepted outside canvas or lost inside canvas");
                        require(same(input.Mouse(),mouse)&&same(input.MouseDelta(),delta)&&same(input.LeftMouseDragDelta(),drag)&&
                            input.MouseDown()==left&&input.MousePressed()==leftPressed&&input.RightMousePressed()==rightPressed&&
                            input.MouseInsideCanvas()==insideBefore,"wheel routing changed pointer/drag/button state");
                        require(input.Down('W')==heldW&&input.Pressed('W')==edgeW&&input.Down('E')==heldE&&input.Pressed('E')==edgeE&&
                            input.Focused()==focusedBefore&&input.FocusLost()==lostBefore,"wheel routing changed keyboard/focus state");
                        require(GetFocus()==priorFocus&&GetCapture()==priorCapture&&GetForegroundWindow()==priorForeground&&!IsWindowVisible(game),"wheel routing changed OS window state");
                    }
                }catch(const std::exception& error){report(error);}
            };
            resize(1024,768,original.left,original.top);
            run("letterbox-top",1024,768,true,true,false,{{512,32,120,0,false}});
            run("letterbox-bottom",1024,768,true,true,false,{{512,736,120,0,false}});
            run("outside-client",1024,768,true,true,false,{{-1,384,120,0,false}});
            run("inside",1024,768,true,true,false,{{512,384,120,1,true}});
            run("positive-half",1024,768,true,true,false,{{512,384,60,.5f,true}});
            run("negative-half",1024,768,true,true,false,{{512,384,-60,-.5f,true}});
            run("fractional-accumulation",1024,768,true,true,false,{{512,384,60,.5f,true},{512,384,60,1,true},{512,384,-120,0,true}});
            run("stale-inside-outside-event",1024,768,true,true,false,{{512,32,120,0,false}});
            run("stale-outside-inside-event",1024,768,false,true,false,{{512,384,120,1,true}});
            run("left-top-inclusive",1024,768,false,true,false,{{0,96,120,1,true}});
            run("right-exclusive",1024,768,true,true,false,{{1024,384,120,0,false}});
            run("bottom-exclusive",1024,768,true,true,false,{{512,672,120,0,false}});
            resize(1280,360,original.left,original.top);
            run("pillarbox-left",1280,360,true,true,false,{{64,180,120,0,false}});
            run("pillarbox-right",1280,360,true,true,false,{{1216,180,120,0,false}});
            run("pillarbox-inside",1280,360,false,true,false,{{640,180,120,1,true}});
            run("pillarbox-right-exclusive",1280,360,true,true,false,{{960,180,120,0,false}});
            resize(1024,768,-4096,-4096);
            run("negative-screen-inside",1024,768,false,true,true,{{512,384,120,1,true}});
            run("negative-screen-outside",1024,768,true,true,true,{{512,32,120,0,false}});
            run("negative-screen-boundary",1024,768,false,true,true,{{0,96,120,1,true}});
            run("focus-loss-inside-event",1024,768,true,false,true,{{512,384,120,0,true}});
            active="bounded-case-accounting";require(wheelCases==20,"wheel case inventory differs");
        }catch(const std::exception& error){report(error);}
        active="restore";try{restore();}catch(const std::exception& error){input=priorInput;report(error);}
        if(wheelFailures==0&&restored)std::cout<<"INPUT_WHEEL_CANVAS_INTEGRATION_PASS cases="<<wheelCases<<" assertions="<<wheelAssertions
            <<" actual_owned_HWND=1 signed_screen_points=1 original_rectangle_restored=1 global_input=0\n";
    }

    void OnStart(Engine& engine) override {
        WheelCanvasContract(engine);
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
        CaptureCancellationContract(engine);
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
        // 若恢復訊息提早到達而漏測背景迴圈，必須失敗，不能以自動修剪誤認通過。
        const auto memoryStats=engine.GetMemoryStats();
        const auto minimizedTrimCount=memoryStats.trimCount;
        // 無更新的外層幀也可能自動修剪；該計數不能代替最小化分支的證據。
        if(minimizedTrimCount<=memoryStats.automaticTrimCount){
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
