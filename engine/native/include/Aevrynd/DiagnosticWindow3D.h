#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace Aevrynd {
enum class EngineWindowMode3D : std::uint8_t { LauncherDefault, OffscreenNoActivateDiagnostic };
namespace DiagnosticWindow3D {
inline constexpr LONG Margin=64;
inline bool HiddenStartup(int command,DWORD flags,WORD startupShow) noexcept {
    return command==SW_HIDE&&(!(flags&STARTF_USESHOWWINDOW)||startupShow==SW_HIDE);
}
// 初始 client 尺寸共用同一政策，避免測試另算一套與 Engine 不相干的縮放。
inline SIZE InitialClientSize(unsigned width,unsigned height,const RECT& workArea,
    int showCommand,DWORD startupFlags,WORD startupShow){
    constexpr auto maximum=(std::numeric_limits<LONG>::max)();
    if(!width||!height||width>static_cast<unsigned>(maximum)||height>static_cast<unsigned>(maximum))
        throw std::runtime_error("Invalid requested client size");
    const auto wideWidth=std::int64_t(workArea.right)-workArea.left;
    const auto wideHeight=std::int64_t(workArea.bottom)-workArea.top;
    if(wideWidth<=0||wideHeight<=0||wideWidth>maximum||wideHeight>maximum)
        throw std::runtime_error("Invalid client work area");
    const auto workWidth=static_cast<LONG>(wideWidth),workHeight=static_cast<LONG>(wideHeight);
    // 隱藏啟動只保留要求值；可見視窗（包括不啟用的可見模式）沿用原工作區縮放。
    if(HiddenStartup(showCommand,startupFlags,startupShow))
        return {static_cast<LONG>(width),static_cast<LONG>(height)};
    const float fit=(std::min)({1.f,float((std::max)(320L,workWidth-48))/width,
        float((std::max)(180L,workHeight-72))/height});
    const float desiredWidth=width*fit,desiredHeight=height*fit;
    if(!std::isfinite(desiredWidth)||!std::isfinite(desiredHeight)||desiredWidth<.5f||desiredHeight<.5f||
       double(desiredWidth)>double(maximum)||double(desiredHeight)>double(maximum))
        throw std::runtime_error("Client size is outside signed window range");
    return {LONG(std::lround(desiredWidth)),LONG(std::lround(desiredHeight))};
}
namespace Detail {
inline bool ReadOwnedHiddenClient(HWND window,RECT& client)noexcept{
    if(!window||!IsWindow(window))return false;
    DWORD process{};const auto thread=GetWindowThreadProcessId(window,&process);
    const auto style=GetWindowLongPtrW(window,GWL_STYLE);
    if(!thread||thread!=GetCurrentThreadId()||process!=GetCurrentProcessId()||
       GetAncestor(window,GA_ROOT)!=window||(style&(WS_CHILD|WS_VISIBLE))||IsWindowVisible(window)||
       IsIconic(window)||GetForegroundWindow()==window||!GetClientRect(window,&client))return false;
    return std::int64_t(client.right)-client.left>=0&&std::int64_t(client.bottom)-client.top>=0;
}
}
// 迴圈只核對，不縮放、不修補；外部 resize 或可見狀態改變必須明確拒絕。
inline bool HiddenClientValid(HWND window,LONG width,LONG height)noexcept{
    RECT client{};
    return width>0&&height>0&&Detail::ReadOwnedHiddenClient(window,client)&&
        std::int64_t(client.right)-client.left==width&&std::int64_t(client.bottom)-client.top==height;
}
// 純整數政策供實際 HWND 補差與極端座標測試共用；保留原左上角也必須檢查新右下角。
inline SIZE CorrectedHiddenClientSize(const RECT& outer,const RECT& client,LONG width,LONG height){
    constexpr auto maximum=(std::numeric_limits<LONG>::max)(),minimum=(std::numeric_limits<LONG>::min)();
    const auto clientWidth=std::int64_t(client.right)-client.left;
    const auto clientHeight=std::int64_t(client.bottom)-client.top;
    const auto outerWidth=std::int64_t(outer.right)-outer.left;
    const auto outerHeight=std::int64_t(outer.bottom)-outer.top;
    const auto correctedWidth=outerWidth+width-clientWidth,correctedHeight=outerHeight+height-clientHeight;
    const auto correctedRight=std::int64_t(outer.left)+correctedWidth;
    const auto correctedBottom=std::int64_t(outer.top)+correctedHeight;
    if(width<=0||height<=0||clientWidth<0||clientHeight<0||clientWidth>maximum||clientHeight>maximum||
       outerWidth<=0||outerHeight<=0||outerWidth>maximum||outerHeight>maximum||
       correctedWidth<=0||correctedHeight<=0||correctedWidth>maximum||correctedHeight>maximum||
       correctedRight<minimum||correctedRight>maximum||correctedBottom<minimum||correctedBottom>maximum)
        throw std::runtime_error("Hidden client correction is outside signed window range");
    return {static_cast<LONG>(correctedWidth),static_cast<LONG>(correctedHeight)};
}
// 僅供初次建立後、Renderer 初始化前呼叫；用真外框/client差補一次，不猜DPI或顯示任何視窗。
inline void RequireHiddenClientSize(HWND window,LONG width,LONG height){
    RECT client{};
    if(width<=0||height<=0||!Detail::ReadOwnedHiddenClient(window,client))
        throw std::runtime_error("Exact client size requires an owned hidden window");
    const auto clientWidth=std::int64_t(client.right)-client.left;
    const auto clientHeight=std::int64_t(client.bottom)-client.top;
    if(clientWidth==width&&clientHeight==height)return;
    RECT outer{};
    if(!GetWindowRect(window,&outer))throw std::runtime_error("Hidden window rectangle unavailable");
    const auto corrected=CorrectedHiddenClientSize(outer,client,width,height);
    // 僅這次已驗證的隱藏自窗略過預設追蹤上限；補差後仍以真 client 尺寸決定成功。
    if(!SetWindowPos(window,nullptr,0,0,static_cast<int>(corrected.cx),static_cast<int>(corrected.cy),
        SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSENDCHANGING))
        throw std::runtime_error("Hidden client correction failed");
    if(!HiddenClientValid(window,width,height))throw std::runtime_error("Hidden client size differs from requested size");
}
inline EngineWindowMode3D SelectMode(std::wstring_view option,bool replay,bool costProbe,bool captureFixture){
    if(option.empty())return EngineWindowMode3D::LauncherDefault;
    if(option!=L"offscreen-noactivate"||!replay||!costProbe||captureFixture)
        throw std::runtime_error("Diagnostic window requires isolated measured replay");
    return EngineWindowMode3D::OffscreenNoActivateDiagnostic;
}
inline bool SelectUnsynchronizedPresent(std::wstring_view option,EngineWindowMode3D mode,bool replay,bool costProbe,bool captureFixture){
    if(option.empty())return false;
    if(option!=L"unsynchronized"||mode!=EngineWindowMode3D::OffscreenNoActivateDiagnostic||!replay||!costProbe||captureFixture)
        throw std::runtime_error("Unsynchronized present requires isolated measured offscreen replay");
    return true;
}
inline bool Outside(const RECT& window,const RECT& desktop) noexcept {
    return window.left<window.right&&window.top<window.bottom&&desktop.left<desktop.right&&desktop.top<desktop.bottom&&
        (window.right<=desktop.left||window.left>=desktop.right||window.bottom<=desktop.top||window.top>=desktop.bottom);
}
inline LONG Coordinate(std::int64_t value){
    if(value<(std::numeric_limits<LONG>::min)()||value>(std::numeric_limits<LONG>::max)())
        throw std::runtime_error("Diagnostic window coordinate overflow");
    return static_cast<LONG>(value);
}
inline RECT VirtualDesktop(){
    const auto x=GetSystemMetrics(SM_XVIRTUALSCREEN),y=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const auto width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(width<=0||height<=0)throw std::runtime_error("Diagnostic virtual desktop unavailable");
    return {Coordinate(x),Coordinate(y),Coordinate(std::int64_t(x)+width),Coordinate(std::int64_t(y)+height)};
}
inline RECT Placement(const RECT& desktop,LONG width,LONG height){
    if(desktop.left>=desktop.right||desktop.top>=desktop.bottom||width<=0||height<=0)
        throw std::runtime_error("Invalid diagnostic window placement");
    const auto maximum=std::int64_t((std::numeric_limits<LONG>::max)());
    auto x=std::int64_t(desktop.right)+Margin;
    if(x+width>maximum)x=std::int64_t(desktop.left)-Margin-width;
    const auto y=std::int64_t(desktop.top);
    RECT result{Coordinate(x),Coordinate(y),Coordinate(x+width),Coordinate(y+height)};
    if(!Outside(result,desktop))throw std::runtime_error("Diagnostic window is not outside desktop");
    return result;
}
inline bool Valid(HWND window,LONG width,LONG height){
    RECT actual{},client{};
    const auto extended=GetWindowLongPtrW(window,GWL_EXSTYLE);
    return IsWindow(window)&&IsWindowVisible(window)&&!IsIconic(window)&&GetForegroundWindow()!=window&&
        (extended&WS_EX_NOACTIVATE)&&(extended&WS_EX_TOOLWINDOW)&&!(extended&WS_EX_APPWINDOW)&&
        GetWindowRect(window,&actual)&&Outside(actual,VirtualDesktop())&&GetClientRect(window,&client)&&
        client.right-client.left==width&&client.bottom-client.top==height;
}
// 顯示器拓撲或外部視窗操作改變條件時，先隱藏自己的 HWND，再中止診斷。
inline void RequireValid(HWND window,LONG width,LONG height){
    try{if(Valid(window,width,height))return;}catch(...){ShowWindow(window,SW_HIDE);throw;}
    ShowWindow(window,SW_HIDE);throw std::runtime_error("Offscreen nonactivating window contract failed");
}
}
}
