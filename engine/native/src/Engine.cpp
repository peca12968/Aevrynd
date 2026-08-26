#include "Rift/Engine.h"
#include <windowsx.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace Rift {
Engine::Engine(EngineConfig config):config_(std::move(config)),renderer_(std::make_unique<D3D12Renderer>()){}
Engine::~Engine(){if(game_)game_->OnStop(*this);if(renderer_)renderer_->WaitIdle();}
void Engine::SetTitle(const std::wstring& title){if(hwnd_)SetWindowTextW(hwnd_,title.c_str());}

int Engine::Run(std::unique_ptr<IGame> game,HINSTANCE instance,int showCommand){
    game_=std::move(game);SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW wc{sizeof(wc)};wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=nullptr;wc.lpszClassName=L"RiftEngineWindow";
    if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("RegisterClass failed");
    RECT rect{0,0,LONG(config_.width),LONG(config_.height)};DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;AdjustWindowRectEx(&rect,style,FALSE,0);
    hwnd_=CreateWindowExW(0,wc.lpszClassName,config_.title.c_str(),style,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,this);
    if(!hwnd_)throw std::runtime_error("CreateWindow failed");RECT client{},window{};GetClientRect(hwnd_,&client);GetWindowRect(hwnd_,&window);LONG clientWidth=client.right-client.left,clientHeight=client.bottom-client.top;if(clientWidth!=LONG(config_.width)||clientHeight!=LONG(config_.height))SetWindowPos(hwnd_,nullptr,0,0,(window.right-window.left)+(LONG(config_.width)-clientWidth),(window.bottom-window.top)+(LONG(config_.height)-clientHeight),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);renderer_->Initialize(hwnd_,config_.width,config_.height);ShowWindow(hwnd_,showCommand);UpdateWindow(hwnd_);game_->OnStart(*this);
    using Clock=std::chrono::steady_clock;auto last=Clock::now();double accumulator=0.0;MSG msg{};
    while(running_){
        input_.BeginFrame();
        input_.PollGamepad();
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){running_=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}if(!running_)break;
        auto now=Clock::now();double frame=std::chrono::duration<double>(now-last).count();last=now;if(frame>.25)frame=.25;accumulator+=frame;int updates=0;
        while(accumulator>=config_.fixedStep&&updates<5){game_->OnUpdate(*this,config_.fixedStep);input_.ConsumePresses();time_+=config_.fixedStep;accumulator-=config_.fixedStep;updates++;}
        if(updates==5)accumulator=0;draw_.Clear();game_->OnRender(*this,float(accumulator/config_.fixedStep));renderer_->Render(draw_,{.018f,.045f,.055f,1});
    }
    game_->OnStop(*this);game_.reset();return int(msg.wParam);
}

LRESULT CALLBACK Engine::WindowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam){Engine* self=reinterpret_cast<Engine*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(message==WM_NCCREATE){auto cs=reinterpret_cast<CREATESTRUCTW*>(lParam);self=static_cast<Engine*>(cs->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->hwnd_=hwnd;}return self?self->HandleMessage(message,wParam,lParam):DefWindowProcW(hwnd,message,wParam,lParam);}
LRESULT Engine::HandleMessage(UINT message,WPARAM wParam,LPARAM lParam){switch(message){case WM_CLOSE:running_=false;DestroyWindow(hwnd_);return 0;case WM_DESTROY:PostQuitMessage(0);return 0;case WM_KEYDOWN:case WM_SYSKEYDOWN:input_.SetKey(unsigned(wParam),true);return 0;case WM_KEYUP:case WM_SYSKEYUP:input_.SetKey(unsigned(wParam),false);return 0;case WM_MOUSEMOVE:{RECT client{};GetClientRect(hwnd_,&client);float width=float((std::max)(1L,client.right-client.left)),height=float((std::max)(1L,client.bottom-client.top));input_.SetMouse({float(GET_X_LPARAM(lParam))*config_.width/width,float(GET_Y_LPARAM(lParam))*config_.height/height});return 0;}case WM_LBUTTONDOWN:SetCapture(hwnd_);input_.SetMouseButton(true);return 0;case WM_LBUTTONUP:ReleaseCapture();input_.SetMouseButton(false);return 0;}return DefWindowProcW(hwnd_,message,wParam,lParam);}
}
