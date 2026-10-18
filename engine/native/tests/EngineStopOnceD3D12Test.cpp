#include <Aevrynd/Engine.h>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
struct Observations {unsigned starts{},updates{},renders{},stops{},destructs{},rendersAtInvalidation{},rendersAfterInvalidation{};bool invalidated{};};
class StopGame final:public Aevrynd::IGame {
    Observations& observations_;bool updateThrows_{},stopThrows_{},messageAtStart_{},unsynchronized_{};UINT windowMessage_{};
    void Invalidate(Aevrynd::Engine& engine){
        if(observations_.invalidated)return;
        observations_.invalidated=true;observations_.rendersAtInvalidation=observations_.renders;
        RECT suggested{0,0,640,480};SendMessageW(engine.Window(),windowMessage_,0,reinterpret_cast<LPARAM>(&suggested));
    }
public:
    StopGame(Observations& observations,bool updateThrows,bool stopThrows,UINT windowMessage,bool messageAtStart=false,bool unsynchronized=false):observations_(observations),updateThrows_(updateThrows),stopThrows_(stopThrows),messageAtStart_(messageAtStart),unsynchronized_(unsynchronized),windowMessage_(windowMessage){}
    ~StopGame()override{++observations_.destructs;}
    void OnStart(Aevrynd::Engine& engine)override{
        ++observations_.starts;
        if(engine.WindowMode3D()==Aevrynd::EngineWindowMode3D::OffscreenNoActivateDiagnostic){
            RECT client{};if(!GetClientRect(engine.Window(),&client))throw std::runtime_error("Native diagnostic client missing");
            Aevrynd::DiagnosticWindow3D::RequireValid(engine.Window(),client.right-client.left,client.bottom-client.top);
        }else if(IsWindowVisible(engine.Window()))throw std::runtime_error("Default hidden launcher changed");
        if(windowMessage_&&messageAtStart_)Invalidate(engine);
    }
    void OnUpdate(Aevrynd::Engine& engine,float)override{
        ++observations_.updates;
        if(updateThrows_)throw std::runtime_error("update-first-divergence");
        if(windowMessage_){if(!messageAtStart_&&observations_.renders>0)Invalidate(engine);return;}
        engine.Quit();
    }
    void OnRender(Aevrynd::Engine&,float)override{++observations_.renders;if(observations_.invalidated)++observations_.rendersAfterInvalidation;}
    void OnStop(Aevrynd::Engine& engine)override{
        ++observations_.stops;
        if(unsynchronized_){
            const auto actual=engine.GetRendererStats().lastPresentObservation;
            if(!actual.observed||!actual.serial||actual.syncInterval!=0||actual.flags!=0||!actual.before.visible||actual.before.foreground||!actual.after.visible||actual.after.foreground)
                throw std::runtime_error("Actual observed unsynchronized offscreen present missing");
            std::cout<<"NATIVE_PRESENT_ACTUAL sync="<<actual.syncInterval<<" flags="<<actual.flags<<" serial="<<actual.serial<<" foreground_before="<<actual.before.foreground<<" foreground_after="<<actual.after.foreground<<'\n';
        }
        if(stopThrows_)throw std::runtime_error("stop-first-divergence");
    }
};
void Need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
class OwnedHandle {
    HANDLE value_;
public:
    explicit OwnedHandle(HANDLE value):value_(value){}
    ~OwnedHandle(){if(value_&&value_!=INVALID_HANDLE_VALUE)CloseHandle(value_);}
    OwnedHandle(const OwnedHandle&)=delete;
    OwnedHandle& operator=(const OwnedHandle&)=delete;
    HANDLE Get()const{return value_;}
};
HANDLE InheritedStandard(DWORD id){
    const HANDLE original=GetStdHandle(id);HANDLE inherited{};
    if(original&&original!=INVALID_HANDLE_VALUE){
        Need(DuplicateHandle(GetCurrentProcess(),original,GetCurrentProcess(),&inherited,0,TRUE,DUPLICATE_SAME_ACCESS)!=FALSE,"Duplicate captured standard handle");
    }else{
        Need(id==STD_INPUT_HANDLE,"Captured diagnostic output handle missing");
        SECURITY_ATTRIBUTES attributes{sizeof(attributes),nullptr,TRUE};
        inherited=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,OPEN_EXISTING,0,nullptr);
        Need(inherited!=INVALID_HANDLE_VALUE,"Create isolated null input");
    }
    return inherited;
}
int HiddenChild(const std::string& name){
    // CTest 的啟動資訊不一定隱藏；另建本測試專屬子程序，不放寬引擎防護。
    STARTUPINFOW parentStartup{};GetStartupInfoW(&parentStartup);
    std::cout<<"NATIVE_PARENT_STARTUP flags="<<parentStartup.dwFlags<<" show="<<parentStartup.wShowWindow<<'\n'<<std::flush;
    std::array<wchar_t,32768> path{};
    const DWORD length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    Need(length>0&&length<path.size(),"Exact diagnostic executable path");
    const std::wstring application(path.data(),length);
    std::wstring command=L"\""+application+L"\" --case "+std::wstring(name.begin(),name.end())+L" --hidden-child";
    OwnedHandle input(InheritedStandard(STD_INPUT_HANDLE)),output(InheritedStandard(STD_OUTPUT_HANDLE)),error(InheritedStandard(STD_ERROR_HANDLE));
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
    startup.wShowWindow=SW_HIDE;startup.hStdInput=input.Get();startup.hStdOutput=output.Get();startup.hStdError=error.Get();
    PROCESS_INFORMATION process{};
    Need(CreateProcessW(application.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE,"Create owned hidden diagnostic child");
    OwnedHandle processHandle(process.hProcess),threadHandle(process.hThread);
    const DWORD wait=WaitForSingleObject(processHandle.Get(),45000);
    if(wait!=WAIT_OBJECT_0){
        const BOOL terminated=TerminateProcess(processHandle.Get(),124);
        const DWORD drained=WaitForSingleObject(processHandle.Get(),5000);
        Need(terminated!=FALSE&&drained==WAIT_OBJECT_0,"Owned diagnostic child failed to drain");
        throw std::runtime_error("Owned hidden diagnostic child did not finish within 45 seconds");
    }
    DWORD exit{};Need(GetExitCodeProcess(processHandle.Get(),&exit)!=FALSE,"Owned diagnostic exit code");
    Need(exit<=255,"Bounded diagnostic child exit code");
    return static_cast<int>(exit);
}
void Case(bool updateThrows,bool stopThrows,const char* expected,bool offscreen=false,UINT windowMessage=0,bool messageAtStart=true,bool unsynchronized=false){
    Observations observations;
    std::string error;
    {
        Aevrynd::EngineConfig config{L"Aevrynd stop lifetime diagnostic",320,180};
        config.enableAutomaticMemoryTrim=false;
        if(offscreen){config.windowMode=Aevrynd::EngineWindowMode3D::OffscreenNoActivateDiagnostic;config.width=640;config.height=360;}
        if(unsynchronized){config.enableFrameCostProbe=true;config.enableUnsynchronizedDiagnosticPresent=true;}
        Aevrynd::Engine engine(config);
        try{const int result=engine.Run(std::make_unique<StopGame>(observations,updateThrows,stopThrows,windowMessage,messageAtStart,unsynchronized),GetModuleHandleW(nullptr),SW_HIDE);Need(result==0,"Normal engine exit");}
        catch(const std::exception& failure){error=failure.what();}
    }
    if(error!=expected)throw std::runtime_error(std::string("Original first divergence: expected=[")+expected+"] actual=["+error+"]");
    Need(observations.starts==1&&observations.stops==1&&observations.destructs==1,"Start/stop/destruction exactly once");
    Need(windowMessage&&messageAtStart?observations.updates==0:observations.updates>0,"Expected update stage reached");
    if(windowMessage){
        Need(observations.invalidated&&observations.rendersAfterInvalidation==0&&observations.renders==observations.rendersAtInvalidation,"Invalid display reached game render callback");
        if(messageAtStart)Need(observations.renders==0,"Early invalidation reached any game render callback");
        else Need(observations.rendersAtInvalidation>0,"Late invalidation did not follow a valid rendered frame");
        std::cout<<"NATIVE_INVALIDATION_STAGE early="<<messageAtStart<<" renders_before="<<observations.rendersAtInvalidation<<" renders_after="<<observations.rendersAfterInvalidation<<'\n';
    }
}
void NonhiddenRejected(){
    Observations observations;std::string error;
    {
        Aevrynd::EngineConfig config{L"Rejected diagnostic launcher",640,360};
        config.windowMode=Aevrynd::EngineWindowMode3D::OffscreenNoActivateDiagnostic;
        Aevrynd::Engine engine(config);
        try{(void)engine.Run(std::make_unique<StopGame>(observations,false,false,0),GetModuleHandleW(nullptr),SW_SHOWNORMAL);}
        catch(const std::exception& failure){error=failure.what();}
        Need(engine.Window()==nullptr,"Rejected launcher created a window");
    }
    Need(error=="Diagnostic window requires hidden launcher"&&observations.starts==0&&observations.updates==0&&
        observations.renders==0&&observations.stops==0&&observations.destructs==1,"Nonhidden launcher did not reject before initialization");
}
}
int main(int argc,char** argv){
    try{
        // 每個 HWND 案例使用獨立程序，避免先前生命週期的視窗訊息影響下一案例。
        Need((argc==3||argc==4)&&std::string(argv[1])=="--case","One explicit isolated case is required");
        const std::string name=argv[2];
        const bool offscreen=name=="offscreen-normal"||name=="offscreen-display-change"||name=="offscreen-dpi-change"||name=="offscreen-nonhidden-reject"||name=="offscreen-display-change-after-render"||name=="offscreen-dpi-change-after-render"||name=="offscreen-unsynchronized-present";
        Need(offscreen||name=="normal"||name=="stop-throw"||name=="update-throw"||name=="both-throw","Known isolated diagnostic case");
        if(argc==4){
            Need(offscreen&&std::string(argv[3])=="--hidden-child","Only diagnostic cases permit hidden child entry");
            STARTUPINFOW startup{};GetStartupInfoW(&startup);
            Need((startup.dwFlags&STARTF_USESHOWWINDOW)!=0&&startup.wShowWindow==SW_HIDE,"Actual child startup must be explicitly hidden");
            std::cout<<"NATIVE_CHILD_STARTUP flags="<<startup.dwFlags<<" show="<<startup.wShowWindow<<'\n'<<std::flush;
        }else if(offscreen)return HiddenChild(name);
        if(name=="normal")Case(false,false,"");
        else if(name=="stop-throw")Case(false,true,"stop-first-divergence");
        else if(name=="update-throw")Case(true,false,"update-first-divergence");
        else if(name=="both-throw")Case(true,true,"update-first-divergence");
        else if(name=="offscreen-normal")Case(false,false,"",true);
        else if(name=="offscreen-unsynchronized-present")Case(false,false,"",true,0,true,true);
        else if(name=="offscreen-display-change")Case(false,false,"Diagnostic display topology changed",true,WM_DISPLAYCHANGE);
        else if(name=="offscreen-dpi-change")Case(false,false,"Diagnostic display topology changed",true,WM_DPICHANGED);
        else if(name=="offscreen-display-change-after-render")Case(false,false,"Diagnostic display topology changed",true,WM_DISPLAYCHANGE,false);
        else if(name=="offscreen-dpi-change-after-render")Case(false,false,"Diagnostic display topology changed",true,WM_DPICHANGED,false);
        else if(name=="offscreen-nonhidden-reject")NonhiddenRejected();
        else throw std::runtime_error("Unknown isolated case");
        const bool rejected=name=="offscreen-nonhidden-reject";
        std::cout<<"ENGINE_STOP_ONCE_NATIVE_PASS case="<<name<<" cases=1 stop_calls="<<(rejected?0:1)
                 <<" first_error_preserved=1 offscreen="<<(offscreen&&!rejected)<<" hidden="<<(!offscreen||rejected)<<" hidden_child="<<(argc==4)<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<"ENGINE_STOP_ONCE_NATIVE_FAIL "<<error.what()<<'\n';return 1;}
}
