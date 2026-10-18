#include <Aevrynd/DiagnosticWindow3D.h>
#include <Aevrynd/Renderer.h>
#include <array>
#include <iostream>
#include <string>
#include <utility>

namespace {
unsigned checks{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void Reject(F work){bool rejected=false;try{work();}catch(const std::exception&){rejected=true;}Need(rejected,"invalid policy accepted");}
void ClientSizePolicy(){
    namespace D=Aevrynd::DiagnosticWindow3D;
    struct Startup{int command;DWORD flags;WORD show;bool hidden;};
    constexpr std::array<Startup,10> startups{{
        {SW_HIDE,0,SW_SHOWNORMAL,true},{SW_HIDE,STARTF_USESHOWWINDOW,SW_HIDE,true},
        {SW_SHOWNORMAL,0,SW_HIDE,false},{SW_SHOW,0,SW_HIDE,false},
        {SW_SHOWNOACTIVATE,0,SW_HIDE,false},{SW_SHOWNA,STARTF_USESHOWWINDOW,SW_SHOWNA,false},
        {SW_HIDE,STARTF_USESHOWWINDOW,SW_SHOWNORMAL,false},
        {SW_HIDE,STARTF_USESHOWWINDOW,SW_SHOWNOACTIVATE,false},
        {SW_SHOWMINNOACTIVE,0,SW_HIDE,false},{SW_SHOWMAXIMIZED,0,SW_HIDE,false}
    }};
    constexpr std::array<SIZE,4> requests{{{640,360},{1280,720},{1920,1080},{719,1001}}};
    constexpr std::array<RECT,6> areas{{{0,0,1920,1080},{0,0,1024,768},{0,0,640,480},
        {0,0,1080,1920},{-1920,-1080,0,0},{0,0,100,100}}};
    unsigned cases{};
    for(const auto requested:requests)for(const auto area:areas)for(const auto startup:startups){
        const auto width=static_cast<unsigned>(requested.cx),height=static_cast<unsigned>(requested.cy);
        const auto actual=D::InitialClientSize(width,height,area,startup.command,startup.flags,startup.show);
        Need(D::HiddenStartup(startup.command,startup.flags,startup.show)==startup.hidden,"startup classification differs from explicit control");
        if(startup.hidden)Need(actual.cx==requested.cx&&actual.cy==requested.cy,"hidden client size depends on work area");
        else{
            // 獨立保留原 Engine 的 float/round 公式，確保可見輸入沒有換演算法。
            const LONG workWidth=area.right-area.left,workHeight=area.bottom-area.top;
            const float fit=(std::min)({1.f,float((std::max)(320L,workWidth-48))/width,
                float((std::max)(180L,workHeight-72))/height});
            Need(actual.cx==LONG(std::lround(width*fit))&&actual.cy==LONG(std::lround(height*fit)),
                "visible initial client differs from original float fit");
        }
        ++cases;
    }
    const RECT work{0,0,1024,768};
    constexpr auto maximum=(std::numeric_limits<LONG>::max)(),minimum=(std::numeric_limits<LONG>::min)();
    for(const auto requested:std::array<std::pair<unsigned,unsigned>,4>{{{0,720},{1280,0},
        {static_cast<unsigned>(maximum)+1u,720},{1280,(std::numeric_limits<unsigned>::max)()}}})
        for(const int show:{SW_HIDE,SW_SHOWNORMAL})Reject([&]{
            (void)D::InitialClientSize(requested.first,requested.second,work,show,0,SW_HIDE);});
    for(const auto area:std::array<RECT,5>{{{0,0,0,100},{100,0,0,100},{0,0,100,0},
        {0,100,100,0},{minimum,0,maximum,100}}})
        for(const int show:{SW_HIDE,SW_SHOWNORMAL})Reject([&]{
            (void)D::InitialClientSize(1280,720,area,show,0,SW_HIDE);});
    const auto signedEdge=D::InitialClientSize(static_cast<unsigned>(maximum),1,work,SW_HIDE,0,SW_HIDE);
    Need(signedEdge.cx==maximum&&signedEdge.cy==1,"hidden signed boundary rounded through float");
    const auto exactLarge=D::InitialClientSize(static_cast<unsigned>(maximum)-1u,720,
        {minimum,0,-1,720},SW_HIDE,STARTF_USESHOWWINDOW,SW_HIDE);
    Need(exactLarge.cx==maximum-1&&exactLarge.cy==720,"wide signed work area overflowed");
    Reject([&]{(void)D::InitialClientSize(static_cast<unsigned>(maximum),1,work,SW_SHOWNORMAL,0,SW_HIDE);});
    Reject([&]{(void)D::InitialClientSize(static_cast<unsigned>(maximum),720,{0,0,maximum,1000},SW_SHOWNORMAL,0,SW_HIDE);});
    const auto corrected=D::CorrectedHiddenClientSize({-100,-200,316,99},{0,0,400,260},1280,720);
    Need(corrected.cx==1296&&corrected.cy==759,"pure hidden correction lost actual nonclient border");
    const auto edge=D::CorrectedHiddenClientSize({maximum-100,minimum,maximum-10,minimum+90},{0,0,90,90},100,100);
    Need(edge.cx==100&&edge.cy==100,"signed endpoint boundary rejected");
    Reject([&]{(void)D::CorrectedHiddenClientSize({maximum-100,0,maximum-10,90},{0,0,90,90},101,90);});
    Reject([&]{(void)D::CorrectedHiddenClientSize({0,maximum-100,90,maximum-10},{0,0,90,90},90,101);});
    Reject([&]{(void)D::CorrectedHiddenClientSize({minimum,0,maximum,90},{0,0,90,90},90,90);});
    Reject([&]{(void)D::CorrectedHiddenClientSize({0,0,90,90},{minimum,0,maximum,90},90,90);});
    Reject([&]{(void)D::CorrectedHiddenClientSize({0,0,90,90},{0,0,1000,90},1,90);});
    Reject([&]{(void)D::CorrectedHiddenClientSize({0,0,90,90},{0,0,90,90},0,90);});
    std::cout<<"INITIAL_CLIENT_SIZE_CPU cases="<<cases<<" visible_legacy_fit=preserved hidden_requested=exact\n";
}
struct HiddenWindowProbe{
    unsigned changes{},clientConstraints{};UINT lastFlags{};bool observe{},constrain{};
    int constrainedWidth{},constrainedHeight{};
};
LRESULT CALLBACK HiddenWindowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
    if(message==WM_NCCREATE){const auto* creation=reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(creation->lpCreateParams));}
    auto* probe=reinterpret_cast<HiddenWindowProbe*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(probe&&message==WM_WINDOWPOSCHANGED){
        const auto* position=reinterpret_cast<const WINDOWPOS*>(lParam);
        if(probe->observe&&!(position->flags&SWP_NOSIZE)){
            ++probe->changes;probe->lastFlags=position->flags;
        }
    }
    if(probe&&probe->constrain&&message==WM_NCCALCSIZE){
        const auto result=DefWindowProcW(window,message,wParam,lParam);
        auto& client=wParam?reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0]:*reinterpret_cast<RECT*>(lParam);
        // 真正限制 client 計算，不依賴可被 NOSENDCHANGING 略過的外框改動訊息。
        const auto right=std::int64_t(client.left)+probe->constrainedWidth;
        const auto bottom=std::int64_t(client.top)+probe->constrainedHeight;
        if(probe->constrainedWidth>=0&&right<client.right)client.right=static_cast<LONG>(right);
        if(probe->constrainedHeight>=0&&bottom<client.bottom)client.bottom=static_cast<LONG>(bottom);
        ++probe->clientConstraints;return result;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
struct HiddenWindowGuard{HWND window{};~HiddenWindowGuard(){if(window)DestroyWindow(window);}};
void HiddenClientWindows(){
    namespace D=Aevrynd::DiagnosticWindow3D;
    const auto instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=HiddenWindowProc;wc.hInstance=instance;wc.lpszClassName=L"AevryndHiddenClientSizePolicyTest";
    Need(RegisterClassW(&wc)!=0,"register hidden client test class");
    constexpr DWORD baseStyle=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    const auto create=[&](SIZE client,DWORD extra,HiddenWindowProbe& probe){
        RECT outer{0,0,client.cx,client.cy};
        Need(AdjustWindowRectEx(&outer,baseStyle|extra,FALSE,0)!=0,"adjust actual caption/resizable border");
        HWND window=CreateWindowExW(0,wc.lpszClassName,L"Hidden client test",baseStyle|extra,
            0,0,outer.right-outer.left,outer.bottom-outer.top,nullptr,nullptr,instance,&probe);
        Need(window!=nullptr,"create hidden caption/resizable window");return window;
    };
    const auto hidden=[&](HWND window){
        Need(!(GetWindowLongPtrW(window,GWL_STYLE)&WS_VISIBLE)&&!IsWindowVisible(window)&&!IsIconic(window)&&
            GetForegroundWindow()!=window,"hidden client test became visible/iconic/foreground");
    };
    const auto requireSize=[&](HWND window,SIZE wanted,DWORD extra,HiddenWindowProbe& probe,const char* label){
        RECT beforeClient{},beforeOuter{};
        const bool beforeClientRead=GetClientRect(window,&beforeClient)!=0;
        const bool beforeOuterRead=GetWindowRect(window,&beforeOuter)!=0;
        const auto report=[&](const char* outcome){
            RECT afterClient{},afterOuter{};
            const bool afterClientRead=GetClientRect(window,&afterClient)!=0;
            const bool afterOuterRead=GetWindowRect(window,&afterOuter)!=0;
            std::cout<<"HIDDEN_CLIENT_OBSERVATION case="<<label<<" resizable="<<bool(extra&WS_THICKFRAME)
                <<" requested="<<wanted.cx<<','<<wanted.cy
                <<" initial_client_ok="<<beforeClientRead<<" initial_client="<<beforeClient.left<<','<<beforeClient.top<<','<<beforeClient.right<<','<<beforeClient.bottom
                <<" initial_outer_ok="<<beforeOuterRead<<" initial_outer="<<beforeOuter.left<<','<<beforeOuter.top<<','<<beforeOuter.right<<','<<beforeOuter.bottom
                <<" after_client_ok="<<afterClientRead<<" after_client="<<afterClient.left<<','<<afterClient.top<<','<<afterClient.right<<','<<afterClient.bottom
                <<" after_outer_ok="<<afterOuterRead<<" after_outer="<<afterOuter.left<<','<<afterOuter.top<<','<<afterOuter.right<<','<<afterOuter.bottom
                <<" probe_changes="<<probe.changes<<" probe_flags="<<probe.lastFlags
                <<" style="<<GetWindowLongPtrW(window,GWL_STYLE)<<" visible="<<bool(IsWindowVisible(window))
                <<" iconic="<<bool(IsIconic(window))<<" foreground="<<(GetForegroundWindow()==window)
                <<" outcome="<<outcome<<'\n'<<std::flush;
        };
        try{D::RequireHiddenClientSize(window,wanted.cx,wanted.cy);report("accepted");}
        catch(const std::exception& error){report("rejected");std::cout<<"HIDDEN_CLIENT_EXCEPTION "<<error.what()<<'\n'<<std::flush;throw;}
    };
    unsigned windows{};
    for(const DWORD extra:{DWORD(0),DWORD(WS_THICKFRAME|WS_MAXIMIZEBOX)})
    for(const auto requested:std::array<SIZE,3>{{{640,360},{1280,720},{719,1001}}}){
        const auto wanted=D::InitialClientSize(static_cast<unsigned>(requested.cx),static_cast<unsigned>(requested.cy),
            {0,0,1024,768},SW_HIDE,STARTF_USESHOWWINDOW,SW_HIDE);
        HiddenWindowProbe probe;HiddenWindowGuard guard{create(wanted,extra,probe)};
        hidden(guard.window);probe.observe=true;requireSize(guard.window,wanted,extra,probe,"requested-client");
        RECT client{};Need(GetClientRect(guard.window,&client)!=0&&client.right-client.left==wanted.cx&&
            client.bottom-client.top==wanted.cy,"actual hidden client did not match requested dimensions");
        Need(D::HiddenClientValid(guard.window,wanted.cx,wanted.cy),"pure hidden client validation rejected exact HWND");
        probe.changes=0;D::RequireHiddenClientSize(guard.window,wanted.cx,wanted.cy);
        Need(probe.changes==0,"already exact hidden HWND was resized again");hidden(guard.window);++windows;
    }
    HiddenWindowProbe probe;HiddenWindowGuard guard{create({320,180},WS_THICKFRAME|WS_MAXIMIZEBOX,probe)};
    hidden(guard.window);probe.observe=true;
    Need(!D::HiddenClientValid(guard.window,1280,720),"correction fixture already has target size");
    RECT original{};Need(GetWindowRect(guard.window,&original)!=0,"read initial hidden window position");
    requireSize(guard.window,{1280,720},WS_THICKFRAME|WS_MAXIMIZEBOX,probe,"forced-initial-drift");
    Need(probe.changes==1,"hidden client correction was not exactly one SetWindowPos");
    constexpr UINT baseCorrectionFlags=SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE;
    constexpr UINT correctionFlags=baseCorrectionFlags|SWP_NOSENDCHANGING;
    Need((probe.lastFlags&correctionFlags)==correctionFlags&&
        !(probe.lastFlags&(SWP_SHOWWINDOW|SWP_HIDEWINDOW|SWP_ASYNCWINDOWPOS)),"hidden correction changed display/activation policy");
    RECT corrected{};Need(GetWindowRect(guard.window,&corrected)!=0&&corrected.left==original.left&&
        corrected.top==original.top,"hidden correction moved its window");
    Need(D::HiddenClientValid(guard.window,1280,720),"corrected hidden HWND is not exact");
    const auto priorChanges=probe.changes;
    Need(!D::HiddenClientValid(guard.window,1279,720)&&probe.changes==priorChanges,"pure validation repaired drift");
    for(const auto wanted:std::array<SIZE,4>{{{0,720},{1280,0},{-1,720},{1280,-1}}}){
        Need(!D::HiddenClientValid(guard.window,wanted.cx,wanted.cy),"invalid exact-client request accepted");
        Reject([&]{D::RequireHiddenClientSize(guard.window,wanted.cx,wanted.cy);});
    }
    constexpr auto maximum=(std::numeric_limits<LONG>::max)();
    Reject([&]{D::RequireHiddenClientSize(guard.window,maximum,720);});
    Reject([&]{D::RequireHiddenClientSize(guard.window,1280,maximum);});
    Need(probe.changes==priorChanges,"invalid/overflow request reached SetWindowPos");
    Need(!D::HiddenClientValid(nullptr,1280,720),"null hidden HWND accepted");
    Reject([&]{D::RequireHiddenClientSize(nullptr,1280,720);});
    Need(!D::HiddenClientValid(GetDesktopWindow(),1280,720),"foreign desktop HWND accepted");
    Reject([&]{D::RequireHiddenClientSize(GetDesktopWindow(),1280,720);});
    HiddenWindowGuard child{CreateWindowExW(0,wc.lpszClassName,L"Hidden child",WS_CHILD,0,0,100,100,
        guard.window,nullptr,instance,nullptr)};
    Need(child.window!=nullptr,"create hidden child negative control");
    Need(!D::HiddenClientValid(child.window,100,100),"hidden child accepted as root window");
    Reject([&]{D::RequireHiddenClientSize(child.window,100,100);});
    const auto destroyed=child.window;Need(DestroyWindow(child.window)!=0,"destroy negative-control HWND");child.window=nullptr;
    Need(!D::HiddenClientValid(destroyed,100,100),"destroyed hidden HWND accepted");
    Reject([&]{D::RequireHiddenClientSize(destroyed,100,100);});
    HiddenWindowProbe minimizedProbe;HiddenWindowGuard minimized{
        create({320,180},WS_THICKFRAME|WS_MAXIMIZEBOX|WS_MINIMIZE,minimizedProbe)};
    Need(!IsWindowVisible(minimized.window)&&IsIconic(minimized.window),"hidden minimized negative control invalid");
    Need(!D::HiddenClientValid(minimized.window,320,180),"iconic hidden HWND accepted");
    Reject([&]{D::RequireHiddenClientSize(minimized.window,320,180);});
    // 外部 resize 後，loop用純驗證中止，不能自動把它補回原尺寸。
    Need(SetWindowPos(guard.window,nullptr,0,0,500,400,baseCorrectionFlags)!=0,"prepare own hidden resize drift");
    const auto driftChanges=probe.changes;
    Need(!D::HiddenClientValid(guard.window,1280,720)&&!D::HiddenClientValid(guard.window,1280,720)&&
        probe.changes==driftChanges,"loop validation swallowed external resize");
    RECT constrained{};Need(GetClientRect(guard.window,&constrained)!=0,"read constrained negative-control client size");
    probe.changes=0;probe.constrain=true;probe.constrainedWidth=constrained.right-constrained.left;
    probe.constrainedHeight=constrained.bottom-constrained.top;
    Reject([&]{D::RequireHiddenClientSize(guard.window,1280,720);});
    Need(probe.changes==1&&probe.clientConstraints>0&&!D::HiddenClientValid(guard.window,1280,720),
        "client-constrained correction retried, bypassed fixture, or claimed exact success");
    hidden(guard.window);
    std::cout<<"HIDDEN_CLIENT_WIN32_PASS windows="<<windows+1<<" requested=1280x720 correction_max=1 visible=0 foreground=0 gpu=0\n";
}
}
int main(){try{
    namespace D=Aevrynd::DiagnosticWindow3D;
    using Mode=Aevrynd::EngineWindowMode3D;
    Need(D::HiddenStartup(SW_HIDE,0,SW_SHOWNORMAL),"explicit hidden default startup rejected");
    Need(D::HiddenStartup(SW_HIDE,STARTF_USESHOWWINDOW,SW_HIDE),"sealed hidden startup rejected");
    Need(!D::HiddenStartup(SW_SHOWNORMAL,0,SW_HIDE),"visible show command accepted");
    Need(!D::HiddenStartup(SW_HIDE,STARTF_USESHOWWINDOW,SW_SHOWNORMAL),"startup override can activate");
    // 真實分歧：隱藏擷取的明確 1280x720 不得因工作區變小而縮成 976x549。
    const RECT smallWorkArea{0,0,1024,768};
    const auto hiddenSize=D::InitialClientSize(1280,720,smallWorkArea,SW_HIDE,STARTF_USESHOWWINDOW,SW_HIDE);
    Need(hiddenSize.cx==1280&&hiddenSize.cy==720,"hidden requested client size was scaled to work area");
    const auto visibleSize=D::InitialClientSize(1280,720,smallWorkArea,SW_SHOWNORMAL,0,SW_SHOWNORMAL);
    Need(visibleSize.cx==976&&visibleSize.cy==549,"visible work-area fit changed");
    ClientSizePolicy();
    Need(D::SelectMode(L"",false,false,false)==Mode::LauncherDefault,"default changed");
    Need(D::SelectMode(L"offscreen-noactivate",true,true,false)==Mode::OffscreenNoActivateDiagnostic,"explicit legal diagnostic rejected");
    Need(!D::SelectUnsynchronizedPresent(L"",Mode::LauncherDefault,false,false,false),"default synchronized present changed");
    Need(D::SelectUnsynchronizedPresent(L"unsynchronized",Mode::OffscreenNoActivateDiagnostic,true,true,false),"explicit measured present diagnostic rejected");
    for(const auto option:{L"0",L"1",L" unsynchronized",L"unsynchronized ",L"UNSYNCHRONIZED",L"unsynchronized0"})
        Reject([&]{(void)D::SelectUnsynchronizedPresent(option,Mode::OffscreenNoActivateDiagnostic,true,true,false);});
    for(unsigned flags=0;flags<8;++flags)if(flags!=3)
        Reject([&]{(void)D::SelectUnsynchronizedPresent(L"unsynchronized",Mode::OffscreenNoActivateDiagnostic,bool(flags&1),bool(flags&2),bool(flags&4));});
    Reject([&]{(void)D::SelectUnsynchronizedPresent(L"unsynchronized",Mode::LauncherDefault,true,true,false);});
    Reject([&]{(void)D::SelectUnsynchronizedPresent(L"unsynchronized",static_cast<Mode>(255),true,true,false);});
    Aevrynd::DrawList draw;
    Need(!draw.UnsynchronizedDiagnosticPresent3D()&&!draw.SetUnsynchronizedDiagnosticPresent3D(true),"unobserved diagnostic present accepted");
    draw.SetPresentObservationEnabled3D(true);
    Need(draw.SetUnsynchronizedDiagnosticPresent3D(true)&&draw.UnsynchronizedDiagnosticPresent3D(),"observed diagnostic annotation rejected");
    draw.SetPresentObservationEnabled3D(false);
    Need(!draw.UnsynchronizedDiagnosticPresent3D(),"unobserved annotation retained");
    draw.SetPresentObservationEnabled3D(true);Need(draw.SetUnsynchronizedDiagnosticPresent3D(true),"second observed annotation rejected");
    draw.Clear();Need(!draw.UnsynchronizedDiagnosticPresent3D()&&!draw.PresentObservationEnabled3D(),"diagnostic present leaked to next frame");
    for(const auto option:{L"0",L"1",L"01",L" offscreen-noactivate",L"offscreen-noactivate ",L"OFFSCREEN-NOACTIVATE"})
        Reject([&]{(void)D::SelectMode(option,true,true,false);});
    for(unsigned flags=0;flags<8;++flags)if(flags!=3)
        Reject([&]{(void)D::SelectMode(L"offscreen-noactivate",bool(flags&1),bool(flags&2),bool(flags&4));});
    for(const RECT desktop: {RECT{0,0,1920,1080},RECT{-3840,-1080,1920,2160},RECT{-1920,-1080,0,0}}){
        const auto rect=D::Placement(desktop,1296,759);
        Need(rect.left==desktop.right+64&&rect.right-rect.left==1296&&rect.bottom-rect.top==759&&D::Outside(rect,desktop),"actual placement recipe");
        Need(!D::Outside({desktop.right-1,desktop.top,desktop.right+1,desktop.bottom},desktop),"partial intersection accepted");
    }
    Reject([]{(void)D::Placement({0,0,0,100},100,100);});
    Reject([]{(void)D::Placement({0,100,100,0},100,100);});
    Reject([]{(void)D::Placement({0,0,100,100},0,100);});
    Reject([]{(void)D::Placement({0,0,100,100},100,-1);});
    constexpr auto maximum=(std::numeric_limits<LONG>::max)(),minimum=(std::numeric_limits<LONG>::min)();
    const auto left=D::Placement({0,0,maximum,100},100,100);
    Need(left.right==-64&&D::Outside(left,{0,0,maximum,100}),"checked left fallback");
    Reject([&]{(void)D::Placement({minimum,0,maximum,100},100,100);});
    Reject([&]{(void)D::Placement({0,maximum-10,100,maximum},100,100);});
    // 真 Win32 HWND，只在桌面外建立，不初始化 D3D、不使用使用者的視窗。
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HiddenClientWindows();
    const auto desktop=D::VirtualDesktop();const auto rect=D::Placement(desktop,400,260);
    const auto instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=instance;wc.lpszClassName=L"AevryndDiagnosticWindowPolicyTest";
    Need(RegisterClassW(&wc)!=0,"register own window");
    HWND hwnd=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,wc.lpszClassName,L"Diagnostic test",WS_OVERLAPPED,
        rect.left,rect.top,400,260,nullptr,nullptr,instance,nullptr);
    Need(hwnd!=nullptr,"create own outside window");
    struct Guard{HWND hwnd;~Guard(){DestroyWindow(hwnd);}}guard{hwnd};
    ShowWindow(hwnd,SW_HIDE);
    Need(!IsWindowVisible(hwnd),"hidden launcher control");
    ShowWindow(hwnd,SW_SHOWNOACTIVATE);
    RECT client{};Need(GetClientRect(hwnd,&client)!=0,"real client rectangle");
    const auto width=client.right-client.left,height=client.bottom-client.top;
    Need(D::Valid(hwnd,width,height),"real noactivate outside window");
    Need(!D::HiddenClientValid(hwnd,width,height),"visible offscreen HWND accepted as hidden exact window");
    Reject([&]{D::RequireHiddenClientSize(hwnd,width,height);});
    Need(D::Valid(hwnd,width,height),"rejected hidden correction altered existing visible offscreen window");
    Need(!D::Valid(hwnd,width+1,height),"client drift accepted");
    ShowWindow(hwnd,SW_HIDE);Need(!D::Valid(hwnd,width,height),"hidden accepted");
    Reject([&]{D::RequireValid(hwnd,width,height);});
    ShowWindow(hwnd,SW_SHOWNOACTIVATE);
    SetWindowLongPtrW(hwnd,GWL_EXSTYLE,WS_EX_TOOLWINDOW);Need(!D::Valid(hwnd,width,height),"noactivate removed accepted");
    SetWindowLongPtrW(hwnd,GWL_EXSTYLE,WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_APPWINDOW);
    Need(!D::Valid(hwnd,width,height),"taskbar window accepted");
    Need(GetForegroundWindow()!=hwnd,"own window acquired focus");
    std::cout<<"DIAGNOSTIC_WINDOW_POLICY_PASS checks="<<checks<<" gpu=0 offscreen=1 foreground=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"DIAGNOSTIC_WINDOW_POLICY_FAIL "<<error.what()<<'\n';return 1;}}
