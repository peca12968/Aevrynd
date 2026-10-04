#include "MistwatchSaveFile.h"
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
unsigned checks{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
std::string Bytes(const std::filesystem::path& file){
    std::ifstream input(file,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
void WriteFixture(const std::filesystem::path& file,const std::string& text){
    std::ofstream output(file,std::ios::binary);output.write(text.data(),static_cast<std::streamsize>(text.size()));
    output.close();Require(bool(output),"isolated fixture write failed");
}
void NoTemporary(const std::filesystem::path& directory){
    for(const auto& entry:std::filesystem::recursive_directory_iterator(directory))
        Require(entry.path().extension()!=L".tmp","failed save leaked temporary file");
}
void Run(const std::filesystem::path& directory){
    using namespace Mistwatch;
    Require(!std::filesystem::exists(directory),"preserve existing test evidence directory");
    std::filesystem::create_directories(directory);
    const auto target=directory/L"player-save.txt";
    SaveGame game;
    Require(WriteSaveFile(target,game)==SaveFileResult::Saved,"initial save failed");
    const auto initial=game.Serialize();Require(Bytes(target)==initial,"saved bytes differ from validated snapshot");
    SaveGame restored;Require(ReadSaveFile(target,restored)&&restored.Serialize()==initial,"saved game did not restore");
    auto invalid=game;invalid.encounter.health=4;
    Require(WriteSaveFile(target,invalid)==SaveFileResult::InvalidSnapshot,"invalid snapshot accepted");
    Require(Bytes(target)==initial,"invalid snapshot destroyed old save");NoTemporary(directory);
    Require(WriteSaveFile({},game)==SaveFileResult::InvalidPath,"empty destination accepted");
    const auto child=target/L"child.txt";
    Require(WriteSaveFile(child,game)==SaveFileResult::DirectoryUnavailable,"file parent accepted as directory");
    Require(Bytes(target)==initial,"directory failure changed old save");NoTemporary(directory);
    // 真實 Win32 排他鎖阻止替換，不使用假 IO 成功或未執行的錯誤鉤子。
    HANDLE locked=CreateFileW(target.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    Require(locked!=INVALID_HANDLE_VALUE,"could not lock isolated save");
    game.session.health=75;
    const auto lockResult=WriteSaveFile(target,game);
    CloseHandle(locked);
    Require(lockResult==SaveFileResult::ReplaceFailed,"exclusive target was replaced");
    Require(Bytes(target)==initial,"failed replacement destroyed old save");NoTemporary(directory);
    Require(WriteSaveFile(target,game)==SaveFileResult::Saved,"save did not recover after target unlocked");
    Require(Bytes(target)==game.Serialize(),"atomic replacement bytes differ");NoTemporary(directory);
    const auto prior=restored.Serialize();
    for(std::size_t length=0;length<game.Serialize().size();++length){
        const auto broken=directory/L"truncated.txt";
        WriteFixture(broken,game.Serialize().substr(0,length));
        Require(!ReadSaveFile(broken,restored),"truncated file accepted");
        Require(restored.Serialize()==prior,"failed parse mutated output candidate");
    }
    const auto oversized=directory/L"oversized.txt";WriteFixture(oversized,std::string(SaveGame::MaximumBytes+1,' '));
    Require(!ReadSaveFile(oversized,restored)&&restored.Serialize()==prior,"oversized read changed output candidate");
    Require(!ReadSaveFile(directory/L"missing.txt",restored)&&restored.Serialize()==prior,"missing read changed output candidate");
    NoTemporary(directory);
    std::cout<<"SAVE_FILE_IO_PASS checks="<<checks<<" atomic_replace=1 invalid_snapshot_preserves_old=1 real_lock_failure_preserves_old=1 user_save_touched=0\n";
}
}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=2)throw std::runtime_error("requires isolated evidence root");
    const auto directory=std::filesystem::path(argv[1])/(L"run-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    Run(directory);std::cout<<"EVIDENCE_DIRECTORY="<<directory.string()<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
