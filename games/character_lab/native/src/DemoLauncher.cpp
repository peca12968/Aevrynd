#include <Windows.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib,"bcrypt.lib")

namespace {
std::string Sha256(const std::filesystem::path& path){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};DWORD objectBytes{},received{};std::vector<unsigned char> object,digest(32);std::array<unsigned char,65536> buffer{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectBytes),sizeof(objectBytes),&received,0)<0)throw std::runtime_error("hash init");
    object.resize(objectBytes);if(BCryptCreateHash(algorithm,&hash,object.data(),DWORD(object.size()),nullptr,0,0)<0)throw std::runtime_error("hash create");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("manifest missing");while(input){input.read(reinterpret_cast<char*>(buffer.data()),buffer.size());if(input.gcount()&&BCryptHashData(hash,buffer.data(),ULONG(input.gcount()),0)<0)throw std::runtime_error("hash update");}if(!input.eof()||BCryptFinishHash(hash,digest.data(),DWORD(digest.size()),0)<0)throw std::runtime_error("hash finish");BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
    constexpr char hex[]="0123456789abcdef";std::string result;for(unsigned char value:digest){result.push_back(hex[value>>4]);result.push_back(hex[value&15]);}return result;
}
struct CurrentPointer{std::string releaseId,manifestHash;};
CurrentPointer ReadPointer(const std::filesystem::path& path){std::ifstream input(path,std::ios::binary);std::string line;CurrentPointer result;bool schema=false;std::size_t bytes{};while(std::getline(input,line)){bytes+=line.size();if(bytes>4096)throw std::runtime_error("pointer too large");if(!line.empty()&&line.back()=='\r')line.pop_back();const auto separator=line.find('=');if(separator==std::string::npos)throw std::runtime_error("pointer syntax");const auto key=line.substr(0,separator),value=line.substr(separator+1);if(key=="schema"){if(schema||value!="1")throw std::runtime_error("pointer schema");schema=true;}else if(key=="releaseId"){if(!result.releaseId.empty())throw std::runtime_error("duplicate release id");result.releaseId=value;}else if(key=="manifestSha256"){if(!result.manifestHash.empty())throw std::runtime_error("duplicate manifest hash");result.manifestHash=value;}else throw std::runtime_error("unknown pointer field");}if(!schema||result.releaseId.empty()||!std::regex_match(result.manifestHash,std::regex("[0-9a-f]{64}")))throw std::runtime_error("pointer incomplete");return result;}
bool ValidId(const std::string& value){return std::regex_match(value,std::regex("[A-Za-z0-9._+\\-]{1,96}"));}
std::wstring Quote(const std::wstring& value){std::wstring result=L"\"";unsigned slashes=0;for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}if(c==L'\"'){result.append(slashes*2+1,L'\\');result.push_back(c);slashes=0;continue;}result.append(slashes,L'\\');slashes=0;result.push_back(c);}result.append(slashes*2,L'\\');result.push_back(L'\"');return result;}
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    std::filesystem::path diagnosticRoot;
    try{
        std::array<wchar_t,32768> module{};const DWORD length=GetModuleFileNameW(nullptr,module.data(),DWORD(module.size()));if(!length||length>=module.size())throw std::runtime_error("launcher path");const std::filesystem::path launcher=std::filesystem::weakly_canonical(std::filesystem::path(std::wstring_view(module.data(),length)));const auto root=launcher.parent_path();diagnosticRoot=root;std::error_code ignored;std::filesystem::remove(root/L"character-launcher-error.txt",ignored);
        const std::wstring stem=launcher.stem().wstring();std::string releaseId,expectedHash;if(stem==L"Aevrynd-Character-Lab"){const auto pointer=ReadPointer(root/L"current"/L"character-lab.current");releaseId=pointer.releaseId;expectedHash=pointer.manifestHash;}else{const std::wstring prefix=L"Aevrynd-Character-Lab-";if(!stem.starts_with(prefix))throw std::runtime_error("launcher name");releaseId=std::filesystem::path(stem.substr(prefix.size())).string();}
        if(!ValidId(releaseId))throw std::runtime_error("release id");const auto releaseRoot=std::filesystem::weakly_canonical(root/L"releases"/L"character-lab"/releaseId);const auto canonicalBase=std::filesystem::weakly_canonical(root/L"releases"/L"character-lab");if(releaseRoot.parent_path()!=canonicalBase)throw std::runtime_error("release escape");const auto manifest=releaseRoot/L"release.manifest";if(!std::filesystem::is_regular_file(manifest)||(!expectedHash.empty()&&Sha256(manifest)!=expectedHash))throw std::runtime_error("manifest hash");
        const auto runtime=releaseRoot/L"Aevrynd-Character-Lab.runtime.exe";if(!std::filesystem::is_regular_file(runtime))throw std::runtime_error("runtime missing");int argumentCount=0;wchar_t** arguments=CommandLineToArgvW(GetCommandLineW(),&argumentCount);if(!arguments)throw std::runtime_error("arguments");std::wstring command=Quote(runtime.wstring());for(int index=1;index<argumentCount;++index){command.push_back(L' ');command+=Quote(arguments[index]);}LocalFree(arguments);const auto stateRoot=root/L"state"/L"character-lab"/releaseId;std::filesystem::create_directories(stateRoot);SetEnvironmentVariableA("AEVRYND_CHARACTER_RELEASE_ID",releaseId.c_str());SetEnvironmentVariableW(L"AEVRYND_CHARACTER_STATE_ROOT",stateRoot.c_str());
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};std::vector<wchar_t> writable(command.begin(),command.end());writable.push_back(0);if(!CreateProcessW(runtime.c_str(),writable.data(),nullptr,nullptr,FALSE,0,nullptr,releaseRoot.c_str(),&startup,&process))throw std::runtime_error("runtime launch");WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);return int(code);
    }catch(const std::exception& error){if(!diagnosticRoot.empty())std::ofstream(diagnosticRoot/L"character-launcher-error.txt",std::ios::binary|std::ios::trunc)<<error.what()<<'\n';return 1;}catch(...){return 2;}
}
