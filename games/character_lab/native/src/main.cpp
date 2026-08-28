#include "CharacterLabGame.h"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

#pragma comment(lib,"bcrypt.lib")

namespace {
constexpr std::array<const wchar_t*,12> CaptureNames={L"front-pair.png",L"female-profile.png",L"back-pair.png",L"male-profile.png",L"portrait-pair.png",L"female-face-front.png",L"male-face-front.png",L"equipment-front.png",L"equipment-swapped.png",L"locomotion-walk.png",L"locomotion-run.png",L"weapon-layer.png"};

class NamedMutexLock {
public:
    explicit NamedMutexLock(const wchar_t* name):handle_(CreateMutexW(nullptr,FALSE,name)){
        if(!handle_)throw std::runtime_error("Capture mutex creation failed");
        const DWORD result=WaitForSingleObject(handle_,300000);
        abandoned_=result==WAIT_ABANDONED;
        if(result!=WAIT_OBJECT_0&&!abandoned_){CloseHandle(handle_);handle_=nullptr;throw std::runtime_error("Capture mutex wait failed");}
    }
    ~NamedMutexLock(){if(handle_){ReleaseMutex(handle_);CloseHandle(handle_);}}
    NamedMutexLock(const NamedMutexLock&)=delete;NamedMutexLock& operator=(const NamedMutexLock&)=delete;
private:HANDLE handle_{};
    bool abandoned_{};
};

std::string Sha256(const std::filesystem::path& path){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};DWORD objectBytes{},resultBytes{};std::vector<unsigned char> object,digest(32);std::array<unsigned char,65536> bytes{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectBytes),sizeof(objectBytes),&resultBytes,0)<0)throw std::runtime_error("SHA-256 initialization failed");
    object.resize(objectBytes);if(BCryptCreateHash(algorithm,&hash,object.data(),DWORD(object.size()),nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("SHA-256 creation failed");}
    std::ifstream input(path,std::ios::binary);if(!input){BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("SHA-256 input open failed");}
    while(input){input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());if(input.gcount()&&BCryptHashData(hash,bytes.data(),ULONG(input.gcount()),0)<0){BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("SHA-256 update failed");}}
    if(!input.eof()||BCryptFinishHash(hash,digest.data(),DWORD(digest.size()),0)<0){BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("SHA-256 finalization failed");}BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
    constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);for(unsigned char value:digest){result.push_back(hex[value>>4]);result.push_back(hex[value&15]);}return result;
}

std::wstring ScopedMutexName(const wchar_t* purpose,const std::filesystem::path& root){
    std::uint64_t value=14695981039346656037ull;for(wchar_t character:root.native()){value^=std::uint64_t(std::towlower(character));value*=1099511628211ull;}wchar_t suffix[17]{};swprintf_s(suffix,L"%016llx",static_cast<unsigned long long>(value));return std::wstring(L"Global\\Aevrynd-")+purpose+L"-"+suffix;
}

void ValidateRelease(const std::filesystem::path& root,std::string_view expectedId){
    if(expectedId.empty())return;const auto manifest=root/L"release.manifest";if(!std::filesystem::is_regular_file(manifest))throw std::runtime_error("Release manifest is required");std::ifstream input(manifest,std::ios::binary);std::string line;bool schema=false,runtime=false,id=false;while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line=="schema=1"){schema=true;continue;}if(line==std::string("releaseId=")+std::string(expectedId)){id=true;continue;}if(!line.starts_with("file="))continue;const auto sizeAt=line.find(" size="),hashAt=line.find(" sha256=");if(sizeAt==std::string::npos||hashAt==std::string::npos||hashAt<=sizeAt)throw std::runtime_error("Release manifest entry is malformed");const std::string relative=line.substr(5,sizeAt-5);if(relative.empty()||relative.find("..")!=std::string::npos||relative.front()=='/'||relative.front()=='\\')throw std::runtime_error("Release manifest path is unsafe");const auto file=std::filesystem::weakly_canonical(root/std::filesystem::path(relative));if(file.parent_path()!=root&&file.native().compare(0,(root.native()+L"\\").size(),root.native()+L"\\")!=0)throw std::runtime_error("Release manifest path escaped");if(!std::filesystem::is_regular_file(file))throw std::runtime_error("Release file is missing");const auto declaredSize=std::stoull(line.substr(sizeAt+6,hashAt-(sizeAt+6)));if(std::filesystem::file_size(file)!=declaredSize||Sha256(file)!=line.substr(hashAt+8))throw std::runtime_error("Release file validation failed");runtime|=relative=="Aevrynd-Character-Lab.runtime.exe";}if(!schema||!id||!runtime)throw std::runtime_error("Release manifest contract failed");
}

std::string EscapeJson(std::string_view value){std::string result;for(char character:value){if(character=='\\'||character=='\"'){result.push_back('\\');result.push_back(character);}else if(static_cast<unsigned char>(character)>=0x20)result.push_back(character);}return result;}
void WriteErrorReceipt(const std::filesystem::path& root,const std::wstring& runId,std::string_view message,DWORD win32){
    const auto errors=root/L"character-capture-errors";std::filesystem::create_directories(errors);const std::wstring id=runId.empty()?L"startup-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()):runId;const auto pending=errors/(id+L".pending"),receipt=errors/(id+L".json");FILETIME time{};GetSystemTimeAsFileTime(&time);const std::uint64_t ticks=(std::uint64_t(time.dwHighDateTime)<<32)|time.dwLowDateTime;std::ofstream(pending,std::ios::binary|std::ios::trunc)<<"{\"schema\":1,\"runId\":\""<<std::filesystem::path(id).string()<<"\",\"stage\":\"capture\",\"timestampFileTime\":"<<ticks<<",\"win32\":"<<win32<<",\"message\":\""<<EscapeJson(message)<<"\"}\n";MoveFileExW(pending.c_str(),receipt.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}

struct RetentionStats{std::uint64_t retainedBytes{},deletedBytes{},budgetBytes{};std::size_t retainedRuns{},deletedRuns{},activeLeases{},staleLeases{};bool budgetExceeded{};};
bool OwnedRunName(std::wstring_view name){const auto dash=name.find(L'-');if(dash==std::wstring_view::npos||dash==0||dash+1>=name.size())return false;for(std::size_t index=0;index<name.size();++index)if(index!=dash&&(name[index]<L'0'||name[index]>L'9'))return false;return true;}
bool OwnedTrashName(std::wstring_view name){if(!name.starts_with(L"run-"))return false;name.remove_prefix(4);const auto finalDash=name.rfind(L'-');return finalDash!=std::wstring_view::npos&&OwnedRunName(name.substr(0,finalDash))&&std::all_of(name.begin()+std::ptrdiff_t(finalDash+1),name.end(),[](wchar_t value){return value>=L'0'&&value<=L'9';});}
bool IsReparsePoint(const std::filesystem::path& path){const DWORD attributes=GetFileAttributesW(path.c_str());return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0;}
std::uint64_t DirectoryBytes(const std::filesystem::path& path){std::uint64_t bytes{};for(const auto& entry:std::filesystem::recursive_directory_iterator(path))if(entry.is_regular_file())bytes+=entry.file_size();return bytes;}
RetentionStats ApplyCaptureRetention(const std::filesystem::path& destination,const std::wstring& newRun){
    RetentionStats stats;stats.budgetBytes=512ull*1024ull*1024ull;std::array<wchar_t,32> budget{};const DWORD length=GetEnvironmentVariableW(L"AEVRYND_CAPTURE_BUDGET_MIB",budget.data(),DWORD(budget.size()));if(length&&length<budget.size()){wchar_t* end{};const auto mib=std::wcstoull(budget.data(),&end,10);if(end==budget.data()+length&&mib>=32&&mib<=16384)stats.budgetBytes=mib*1024ull*1024ull;}
    const auto runs=destination/L"runs",trash=destination/L"trash";std::filesystem::create_directories(trash);for(const auto& entry:std::filesystem::directory_iterator(trash)){if(entry.is_directory()&&!IsReparsePoint(entry.path())&&OwnedTrashName(entry.path().filename().native())){std::error_code ignored;std::filesystem::remove_all(entry.path(),ignored);}}
    std::vector<std::filesystem::directory_entry> generations;for(const auto& entry:std::filesystem::directory_iterator(runs))if(entry.is_directory()&&!IsReparsePoint(entry.path())&&OwnedRunName(entry.path().filename().native()))generations.push_back(entry);std::sort(generations.begin(),generations.end(),[](const auto& left,const auto& right){return left.last_write_time()>right.last_write_time();});std::vector<std::uint64_t> sizes;sizes.reserve(generations.size());for(const auto& entry:generations){const auto bytes=DirectoryBytes(entry.path());sizes.push_back(bytes);stats.retainedBytes+=bytes;}
    std::size_t remaining=generations.size();for(std::size_t index=generations.size();index>8&&stats.retainedBytes>stats.budgetBytes;--index){const std::size_t candidate=index-1;if(remaining<=8)break;const auto name=generations[candidate].path().filename().wstring();if(name==newRun)continue;const auto tombstone=trash/(L"run-"+name+L"-"+std::to_wstring(GetTickCount64()));if(!MoveFileExW(generations[candidate].path().c_str(),tombstone.c_str(),MOVEFILE_WRITE_THROUGH)){if(GetLastError()==ERROR_SHARING_VIOLATION||GetLastError()==ERROR_ACCESS_DENIED)++stats.activeLeases;continue;}std::error_code error;std::filesystem::remove_all(tombstone,error);if(!error){stats.retainedBytes-=sizes[candidate];stats.deletedBytes+=sizes[candidate];++stats.deletedRuns;--remaining;}}
    for(const auto& entry:generations)if(std::filesystem::exists(entry.path()))++stats.retainedRuns;stats.budgetExceeded=stats.retainedBytes>stats.budgetBytes;return stats;
}

void PublishCaptures(const std::filesystem::path& staging,const std::filesystem::path& destination,const std::filesystem::path& executable){
    for(const wchar_t* name:CaptureNames)if(!std::filesystem::is_regular_file(staging/name))throw std::runtime_error("Capture staging set is incomplete");
    const std::wstring runId=staging.filename().wstring();const std::string runIdText=staging.filename().string();const std::filesystem::path runs=destination/L"runs",published=runs/runId;
    std::filesystem::create_directories(runs);std::filesystem::rename(staging,published);RetentionStats retention{};retention.budgetBytes=512ull*1024ull*1024ull;retention.retainedRuns=1;retention.retainedBytes=DirectoryBytes(published);
    const std::filesystem::path markerTemp=destination/L"capture-current.pending",marker=destination/L"capture-current.txt";
    {std::ofstream output(markerTemp,std::ios::binary|std::ios::trunc);output<<"schema=1\nrun="<<runIdText<<"\ncount=12\nexe.sha256="<<Sha256(executable)<<"\nretention.retainedRuns="<<retention.retainedRuns<<"\nretention.deletedRuns="<<retention.deletedRuns<<"\nretention.retainedBytes="<<retention.retainedBytes<<"\nretention.deletedBytes="<<retention.deletedBytes<<"\nretention.activeLeases="<<retention.activeLeases<<"\nretention.staleLeases="<<retention.staleLeases<<"\nretention.budgetBytes="<<retention.budgetBytes<<"\nretention.budgetExceeded="<<(retention.budgetExceeded?1:0)<<"\n";const auto releaseManifest=executable.parent_path()/L"release.manifest";if(std::filesystem::is_regular_file(releaseManifest))output<<"release.manifest.sha256="<<Sha256(releaseManifest)<<"\n";for(const wchar_t* name:CaptureNames)output<<"file="<<std::filesystem::path(name).string()<<" size="<<std::filesystem::file_size(published/name)<<" sha256="<<Sha256(published/name)<<'\n';if(!output)throw std::runtime_error("Capture manifest write failed");}
    if(!MoveFileExW(markerTemp.c_str(),marker.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){std::error_code ignored;std::filesystem::remove_all(published,ignored);throw std::runtime_error("Capture manifest publish failed");}
    retention=ApplyCaptureRetention(destination,runId);
    {std::ofstream output(markerTemp,std::ios::binary|std::ios::trunc);output<<"schema=1\nrun="<<runIdText<<"\ncount=12\nexe.sha256="<<Sha256(executable)<<"\nretention.retainedRuns="<<retention.retainedRuns<<"\nretention.deletedRuns="<<retention.deletedRuns<<"\nretention.retainedBytes="<<retention.retainedBytes<<"\nretention.deletedBytes="<<retention.deletedBytes<<"\nretention.activeLeases="<<retention.activeLeases<<"\nretention.staleLeases="<<retention.staleLeases<<"\nretention.budgetBytes="<<retention.budgetBytes<<"\nretention.budgetExceeded="<<(retention.budgetExceeded?1:0)<<"\n";const auto releaseManifest=executable.parent_path()/L"release.manifest";if(std::filesystem::is_regular_file(releaseManifest))output<<"release.manifest.sha256="<<Sha256(releaseManifest)<<"\n";for(const wchar_t* name:CaptureNames)output<<"file="<<std::filesystem::path(name).string()<<" size="<<std::filesystem::file_size(published/name)<<" sha256="<<Sha256(published/name)<<'\n';if(!output)throw std::runtime_error("Capture retention manifest write failed");}if(!MoveFileExW(markerTemp.c_str(),marker.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Capture retention manifest publish failed");
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int showCommand){
    std::filesystem::path executableDirectory,executablePath,stagingDirectory,stateRoot;std::wstring runId;
    std::unique_ptr<NamedMutexLock> captureLock;
    try{
        std::array<wchar_t,32768> executable{};const DWORD length=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));if(!length||length>=executable.size())throw std::runtime_error("Executable path resolution failed");executablePath=std::filesystem::weakly_canonical(std::filesystem::path(std::wstring_view(executable.data(),length)));executableDirectory=executablePath.parent_path();
        stateRoot.clear();std::array<wchar_t,32768> stateBuffer{};const DWORD stateLength=GetEnvironmentVariableW(L"AEVRYND_CHARACTER_STATE_ROOT",stateBuffer.data(),DWORD(stateBuffer.size()));if(stateLength){if(stateLength>=stateBuffer.size())throw std::runtime_error("State path is too long");const std::filesystem::path requested(std::wstring_view(stateBuffer.data(),stateLength));if(!requested.is_absolute())throw std::runtime_error("State path must be absolute");const auto candidate=std::filesystem::weakly_canonical(requested);const std::wstring releasePrefix=executableDirectory.native()+L"\\";if(candidate==executableDirectory||(candidate.native().size()>releasePrefix.size()&&candidate.native().compare(0,releasePrefix.size(),releasePrefix)==0))throw std::runtime_error("State path cannot be inside an immutable release");stateRoot=candidate;}else stateRoot=executableDirectory;
        std::array<char,128> releaseIdBuffer{};const DWORD releaseIdLength=GetEnvironmentVariableA("AEVRYND_CHARACTER_RELEASE_ID",releaseIdBuffer.data(),DWORD(releaseIdBuffer.size()));const std::string releaseId=releaseIdLength&&releaseIdLength<releaseIdBuffer.size()?std::string(releaseIdBuffer.data(),releaseIdLength):std::string{};ValidateRelease(executableDirectory,releaseId);const bool capture=Aevrynd::CommandLineHasOption(commandLine?commandLine:L"",L"--capture-demo");if(capture){const std::wstring captureMutex=ScopedMutexName(L"CapturePublisher",stateRoot);captureLock=std::make_unique<NamedMutexLock>(captureMutex.c_str());const std::filesystem::path stagingRoot=stateRoot/L"character-capture-staging";std::error_code ignored;std::filesystem::remove_all(stagingRoot,ignored);runId=std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());stagingDirectory=stagingRoot/runId;std::filesystem::create_directories(stagingDirectory);}
        const bool enableDLSS=Aevrynd::IsDLSSStartupRequested(commandLine);
        Aevrynd::Engine engine({L"Aevrynd Character Lab",1280,720,1.f/60.f,true,enableDLSS});
        const int result=engine.Run(std::make_unique<CharacterLabGame>(stagingDirectory),instance,showCommand);
        if(capture&&result==0)PublishCaptures(stagingDirectory,stateRoot/L"character-captures",executablePath);
        if(capture){std::error_code ignored;std::filesystem::remove_all(stagingDirectory,ignored);}
        return result;
    }catch(const std::exception& error){
        if(!stagingDirectory.empty()){std::error_code ignored;std::filesystem::remove_all(stagingDirectory,ignored);}
        if(!stateRoot.empty())WriteErrorReceipt(stateRoot,runId,error.what(),GetLastError());return 1;
    }catch(...){
        if(!stagingDirectory.empty()){std::error_code ignored;std::filesystem::remove_all(stagingDirectory,ignored);}
        if(!stateRoot.empty())WriteErrorReceipt(stateRoot,runId,"Unknown native failure",GetLastError());return 2;
    }
}
