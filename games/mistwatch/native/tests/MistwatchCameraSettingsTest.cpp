#include "MistwatchCameraSettings.h"
#include "MistwatchCamera.h"
#include "MistwatchFieldJournal.h"
#include "MistwatchCameraSettingsFile.h"
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
bool Same(float a,float b){return std::bit_cast<unsigned>(a)==std::bit_cast<unsigned>(b);}
bool Same(const Mistwatch::OrbitCamera& a,const Mistwatch::OrbitCamera& b){
    return Same(a.yaw,b.yaw)&&Same(a.pitch,b.pitch)&&Same(a.distance,b.distance);
}
void Settings(){
    using Mistwatch::CameraSettings;
    CameraSettings s;Need(s.Valid()&&s.sensitivityPercent==100&&!s.invertVertical,"DEFAULT_CHANGED");
    Need(!s.Adjust(false,true,false,true,true)&&s.Serialize()=="MISTWATCH_CAMERA 1 100 0","DISALLOWED_EDIT");
    for(unsigned i=0;i<10;++i)s.Adjust(true,true,false,false,false);
    Need(s.sensitivityPercent==25&&!s.Adjust(true,true,false,false,false),"MINIMUM_CHANGED");
    for(unsigned i=0;i<10;++i)s.Adjust(true,false,true,false,false);
    Need(s.sensitivityPercent==200&&!s.Adjust(true,false,true,false,false),"MAXIMUM_CHANGED");
    Need(!s.Adjust(true,true,true,false,false)&&s.sensitivityPercent==200,"OPPOSITE_KEYS_DID_NOT_CANCEL");
    Need(s.Adjust(true,false,false,true,false)&&s.invertVertical,"INVERT_DID_NOT_TOGGLE");
    Need(s.Adjust(true,true,true,true,true)&&s.sensitivityPercent==100&&!s.invertVertical,"RESET_PRIORITY");
    for(unsigned percent=25;percent<=200;++percent)for(bool inverse:{false,true}){
        s={percent,inverse};CameraSettings restored;
        Need(restored.Restore(s.Serialize())&&restored.sensitivityPercent==percent&&restored.invertVertical==inverse,
            "ROUND_TRIP_CHANGED");
        const auto old=restored.Serialize();
        for(const auto invalid:std::array{"","MISTWATCH_CAMERA 2 100 0","MISTWATCH_CAMERA 1 24 0",
            "MISTWATCH_CAMERA 1 201 0","MISTWATCH_CAMERA 1 -1 0","MISTWATCH_CAMERA 1 +100 0",
            "MISTWATCH_CAMERA 1 1e2 0","MISTWATCH_CAMERA 1 100 2","MISTWATCH_CAMERA 1 100 0 extra",
            "MISTWATCH_CAMERA 1 99999999999999999999999999999 0","MISTWATCH_AUDIO 1 100 0"}){
            Need(!restored.Restore(invalid)&&restored.Serialize()==old,"INVALID_SETTINGS_CHANGED_LIVE_STATE");
        }
        Need(!restored.Restore(std::string(129,' '))&&restored.Serialize()==old,"OVERSIZE_ACCEPTED");
    }
}
void MenuBoundaries(){
    for(unsigned mask=0;mask<128;++mask){
        const auto b=[&](unsigned bit){return bool(mask&(1u<<bit));};
        const bool focused=b(0),wasStarted=b(1),wasPaused=b(2),started=b(3),paused=b(4),enter=b(5),escape=b(6);
        const bool expected=focused&&!enter&&!escape&&
            ((!wasStarted&&!started)||(wasStarted&&started&&wasPaused&&paused));
        const bool accepted=Mistwatch::CanEditCameraSettings(focused,wasStarted,wasPaused,started,paused,enter,escape);
        Need(accepted==expected,"MENU_BOUNDARY");
        Mistwatch::CameraSettings s;const auto before=s.Serialize();
        s.Adjust(accepted,false,true,true,false);
        Need(accepted?(s.sensitivityPercent==125&&s.invertVertical):(s.Serialize()==before),"MENU_LEAKED_TO_GAMEPLAY");
    }
}
void RealOrbit(){
    using Aevrynd::Vec2;using Mistwatch::OrbitCamera;
    for(unsigned i=0;i<128;++i){
        OrbitCamera actual;actual.yaw=float(int(i%17)-8)*.31f;actual.pitch=float(i%7)*.15f;actual.distance=2.5f+float(i%13)*.5f;
        const Vec2 drag{float(int(i%9)-4)*7,float(int(i%5)-2)*11};
        const float wheel=float(int(i%7)-3)*.5f,keyboard=float(int(i%3)-1),dt=i%4==0?.25f:i%4==1?0.f:1.f/60.f;
        auto expected=actual;
        expected.yaw=std::remainder(expected.yaw-drag.x*.005f+keyboard*(std::min)(dt,.1f)*1.5f,6.2831853f);
        expected.pitch=std::clamp(expected.pitch+drag.y*.004f,-.15f,1.1f);
        expected.distance=std::clamp(expected.distance-wheel*.7f,2.5f,10.f);
        actual.Update(drag,wheel,keyboard,dt);
        Need(Same(actual,expected),"DEFAULT_NOT_BIT_EXACT");
    }
    for(unsigned percent:{25u,50u,100u,125u,200u})for(bool inverse:{false,true}){
        const Mistwatch::CameraSettings settings{percent,inverse};
        OrbitCamera actual,expected;
        actual.Update({8,9},.5f,1,.01f,false,0,settings.Sensitivity(),inverse);
        expected.Update({8*settings.Sensitivity(),9*settings.Sensitivity()*(inverse?-1.f:1.f)},.5f,1,.01f);
        Need(Same(actual,expected),"SETTINGS_NOT_APPLIED_TO_REAL_ORBIT");
        OrbitCamera keyboard,unmodified;
        keyboard.Update({},1,1,.01f,false,0,settings.Sensitivity(),inverse);unmodified.Update({},1,1,.01f);
        Need(Same(keyboard,unmodified),"SETTINGS_CHANGED_KEYBOARD_OR_ZOOM");
        OrbitCamera center;center.Update({},0,0,.01f,true,1.7f,settings.Sensitivity(),inverse);
        Need(Same(center.yaw,std::remainder(1.7f,6.2831853f)),"SETTINGS_CHANGED_RECENTER");
    }
    for(float bad:{0.f,.249f,2.001f,-1.f,(std::numeric_limits<float>::infinity)(),(std::numeric_limits<float>::quiet_NaN)()}){
        OrbitCamera c;const auto before=c;c.Update({5,7},1,1,.01f,false,0,bad,true);
        Need(Same(c,before),"INVALID_MULTIPLIER_CHANGED_ORBIT");
    }
    for(bool inverse:{false,true}){
        OrbitCamera c;const auto before=c;
        c.Update({(std::numeric_limits<float>::max)(),1},1,1,.01f,false,0,2,inverse);
        Need(Same(c,before),"SCALED_OVERFLOW_CHANGED_ORBIT");
    }
}
void ActualHud(){
    using namespace Mistwatch;
    Aevrynd::DrawList draw;
    Session session;Encounter encounter;ObjectiveGuide guide;guide.valid=true;guide.hasTarget=true;guide.index=0;guide.distance=12;
    ActionFeedbackToast toast;
    unsigned cases{};float widest{};
    for(bool started:{false,true})for(bool paused:{false,true})for(bool journal:{false,true})
        for(unsigned percent:{25u,100u,200u})for(bool inverted:{false,true}){
            auto hud=FieldPresentation::Build(session,guide,encounter,started,paused,journal);
            const auto original=hud;
            const CameraSettings settings{percent,inverted};
            FieldPresentation::PresentCameraSettings(hud,settings,started,paused);
            if(started&&!paused)Need(hud.navigation==original.navigation,"SETTINGS_REPLACED_LIVE_NAVIGATION");
            else Need(hud.navigation==settings.MenuLine(),"MENU_SETTINGS_NOT_VISIBLE");
            Need(hud.controls==original.controls&&hud.context==original.context&&hud.instruction==original.instruction&&
                hud.journal==original.journal,"SETTINGS_REMOVED_EXISTING_CONTROLS_OR_FEEDBACK");
            const auto measured=draw.MeasureSystemText(hud.navigation,7.f*2.6f);
            widest=(std::max)(widest,measured.x);
            Need(std::isfinite(measured.x)&&measured.x<=1240,"ACTUAL_SETTINGS_TEXT_TOO_WIDE");
            const auto layout=FieldPresentation::MeasureLayout(draw,hud,session,"",toast,started,paused,true,false,45);
            Need(layout.bottom.Right()<=1280&&layout.bottom.Bottom()<=720,"SETTINGS_PANEL_ESCAPED_CANVAS");
            FieldPresentation::Draw(draw,hud,session,"",toast,true,false,45,started,paused);
            ++cases;
        }
    auto invalid=FieldPresentation::Hud{};invalid.navigation="keep-invalid-evidence";
    FieldPresentation::PresentCameraSettings(invalid,{},true,true);
    Need(invalid.navigation=="keep-invalid-evidence","INVALID_HUD_WAS_MASKED");
    Need(Aevrynd::DrawList::SystemGlyphCacheStats().residentGlyphs>0,"DID_NOT_USE_ACTUAL_SYSTEM_GLYPHS");
    std::cout<<"CAMERA_SETTINGS_ACTUAL_HUD_PASS cases="<<cases<<" maximum_navigation_pixels="<<widest
        <<" measured_system_glyphs=1 gpu=0\n";
}
void ActualFiles(){
    using namespace Mistwatch;
    namespace fs=std::filesystem;
    const auto root=fs::current_path()/("camera-settings-files-"+std::to_string(GetCurrentProcessId()));
    std::error_code error;
    Need(fs::create_directory(root,error)&&!error,"FRESH_SETTINGS_TEST_DIRECTORY");
    const auto file=root/"profile"/"camera-settings.txt";
    const auto bytes=[](const fs::path& path){std::ifstream in(path,std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());};
    const auto write=[&](const fs::path& path,const std::string& text){
        std::ofstream out(path,std::ios::binary);out.write(text.data(),std::streamsize(text.size()));out.close();
        Need(bool(out),"BAD_SETTINGS_FIXTURE_WRITE");
    };
    CameraSettings live{175,true};const auto original=live.Serialize();
    Need(!ReadCameraSettingsFile(file,live)&&live.Serialize()==original,"MISSING_FILE_CHANGED_LIVE_SETTINGS");
    unsigned roundTrips{};
    for(unsigned percent=25;percent<=200;percent+=25)for(bool inverted:{false,true}){
        const CameraSettings expected{percent,inverted};
        Need(WriteCameraSettingsFile(file,expected),"ACTUAL_SETTINGS_WRITE_FAILED");
        Need(bytes(file)==expected.Serialize(),"ACTUAL_SETTINGS_FILE_DIFFERS");
        CameraSettings fresh;
        Need(ReadCameraSettingsFile(file,fresh)&&fresh.Serialize()==expected.Serialize(),"ACTUAL_SETTINGS_REOPEN_DIFFERS");
        ++roundTrips;
    }
    const auto retained=bytes(file);
    CameraSettings invalid;invalid.sensitivityPercent=201;
    Need(!WriteCameraSettingsFile(file,invalid)&&bytes(file)==retained,"INVALID_WRITE_REPLACED_EXISTING_FILE");
    const auto legacyTemporary=fs::path(file.wstring()+L".tmp");
    write(legacyTemporary,"retained-other-writer");
    const HANDLE other=CreateFileW(legacyTemporary.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Need(other!=INVALID_HANDLE_VALUE,"OTHER_WRITER_FIXTURE");
    const bool independent=WriteCameraSettingsFile(file,{75,true});CloseHandle(other);
    Need(independent&&bytes(legacyTemporary)=="retained-other-writer","WRITER_REUSED_OR_TRUNCATED_SHARED_TEMPORARY");
    const auto beforeLocked=bytes(file);
    const HANDLE locked=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Need(locked!=INVALID_HANDLE_VALUE,"LOCKED_TARGET_FIXTURE");
    const bool replaced=WriteCameraSettingsFile(file,{125,false});CloseHandle(locked);
    Need(!replaced&&bytes(file)==beforeLocked,"LOCKED_TARGET_REPLACED_OR_REPORTED_SAVED");
    Need(WriteCameraSettingsFile(file,{125,false}),"WRITE_DID_NOT_RECOVER_AFTER_TARGET_UNLOCK");
    // 新程序使用自己的預設物件讀取真實檔案；不是拿記憶體中的序列化結果自比。
    const auto childReceipt=root/"restored-in-new-process.txt";
    std::array<wchar_t,32768> executable{};
    const auto pathLength=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
    Need(pathLength>0&&pathLength<executable.size(),"CHILD_TEST_EXECUTABLE_PATH");
    std::wstring command=L"\""+std::wstring(executable.data(),pathLength)+L"\" --read-camera-settings \""+
        file.wstring()+L"\" \""+childReceipt.wstring()+L"\"";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    Need(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),
        &startup,&process)!=0,"INDEPENDENT_SETTINGS_READER_NOT_STARTED");
    CloseHandle(process.hThread);
    const auto waited=WaitForSingleObject(process.hProcess,5000);DWORD exitCode{};
    const bool exitKnown=GetExitCodeProcess(process.hProcess,&exitCode)!=0;CloseHandle(process.hProcess);
    Need(waited==WAIT_OBJECT_0&&exitKnown&&exitCode==0,"INDEPENDENT_SETTINGS_READER_FAILED");
    Need(bytes(childReceipt)=="MISTWATCH_CAMERA 1 125 0","NEW_PROCESS_DID_NOT_RESTORE_SAVED_SETTINGS");
    const auto parentFile=root/"parent-is-a-file";write(parentFile,"keep-parent");
    Need(!WriteCameraSettingsFile(parentFile/"prefs.txt",{50,false})&&bytes(parentFile)=="keep-parent","PARENT_FILE_CHANGED");
    const auto directoryTarget=root/"directory-target";fs::create_directory(directoryTarget);
    Need(!WriteCameraSettingsFile(directoryTarget,{50,false})&&fs::is_directory(directoryTarget),"DIRECTORY_TARGET_CHANGED");
    unsigned malformed{};
    for(const auto& text:std::array<std::string,8>{"","MISTWATCH_CAMERA 9 100 0","MISTWATCH_CAMERA 1 201 0",
        "MISTWATCH_CAMERA 1 100 0\nMISTWATCH_CAMERA 1 50 1",std::string(129,'x'),std::string(4096,' '),
        std::string("MISTWATCH_CAMERA 1 100 0")+char{0},"MISTWATCH_CAMERA 1 nan 0"}){
        write(file,text);live={175,true};
        Need(!ReadCameraSettingsFile(file,live)&&live.Serialize()==original&&bytes(file)==text,
            "MALFORMED_DISK_FILE_CHANGED_LIVE_SETTINGS_OR_WAS_REWRITTEN");++malformed;
    }
    Need(WriteCameraSettingsFile(file,{100,false}),"FINAL_SETTINGS_RESTORE");
    for(const auto& entry:fs::directory_iterator(file.parent_path()))
        Need(!entry.path().filename().wstring().starts_with(L"camera-settings.txt.tmp-"),"OWN_TEMPORARY_LEFT_BEHIND");
    std::cout<<"CAMERA_SETTINGS_ACTUAL_FILES_PASS round_trips="<<roundTrips<<" malformed="<<malformed
        <<" independent_temporary=1 locked_target_preserved=1 parent_failure_preserved=1 new_reader_process=1 root="<<root.string()<<'\n';
}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=1){
        if(argc!=4||std::wstring_view(argv[1])!=L"--read-camera-settings")return 2;
        Mistwatch::CameraSettings restored;
        if(!Mistwatch::ReadCameraSettingsFile(argv[2],restored))return 3;
        std::ofstream out(std::filesystem::path(argv[3]),std::ios::binary);out<<restored.Serialize();out.close();
        return out?0:4;
    }
    try{Settings();MenuBoundaries();RealOrbit();ActualHud();ActualFiles();std::cout<<"CAMERA_SETTINGS_CPU_PASS checks="<<checks
        <<" default_bit_exact=128 preference_round_trips=352 boundary_masks=128 gpu=0 full_game_validation=0\n";}
    catch(const std::exception& e){std::cerr<<"CAMERA_SETTINGS_FIRST_DIVERGENCE: "<<e.what()<<'\n';return 1;}
}
