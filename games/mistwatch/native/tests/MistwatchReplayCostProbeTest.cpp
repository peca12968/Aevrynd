#include "MistwatchReplayCostProbe.h"
#include <objbase.h>
#include <iterator>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
void Need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
Aevrynd::EngineFrameCost3D Frame(std::uint64_t serial){return{serial,2,1,2,3,40,5,51,true};}
Aevrynd::RendererStats Stats(std::uint64_t serial){
    Aevrynd::RendererStats value{};value.lastSubmissionTelemetryReceipt.serial=serial;
    value.lastCompletedGpuTimingReceipt={1,10,20,100,100000000,123};
    value.lastProductionCaptureCopyGpuTiming={1,11,12,100,10000000,true,true};
    return value;
}
std::filesystem::path Directory(){
    GUID guid{};Need(SUCCEEDED(CoCreateGuid(&guid)),"Streaming unique identity");
    std::array<wchar_t,40> text{};Need(StringFromGUID2(guid,text.data(),int(text.size()))>0,"Streaming unique name");
    const auto path=std::filesystem::temp_directory_path()/(std::wstring(L"AevryndReplayStreaming-")+text.data());
    Need(std::filesystem::create_directory(path),"Streaming exclusive directory");return path;
}
std::filesystem::path ChunkPath(const std::filesystem::path& path,std::size_t index){
    std::array<wchar_t,48> name{};swprintf_s(name.data(),name.size(),L"frame-cost.chunk-%03u.bin",static_cast<unsigned>(index));return path/name.data();
}
void Clean(const std::filesystem::path& path,std::size_t chunks){
    // 成功才移除自己建立的明列檔案；失敗保留 GUID 目錄供根代理查證。
    for(std::size_t i=0;i<chunks;++i)std::filesystem::remove(ChunkPath(path,i));
    for(const auto* name:{L"frame-cost.stream",L"frame-cost.tsv",L"frame-cost.txt",L"kept.bin"})std::filesystem::remove(path/name);
    Need(std::filesystem::remove(path),"Streaming private directory clean");
}
void Fill(Mistwatch::ReplayCostProbe& probe,std::uint64_t first,std::uint64_t last,std::uint64_t gpu=1){
    for(auto serial=first;serial<=last;++serial){auto stats=Stats(serial);stats.lastCompletedGpuTimingReceipt.serial=gpu;stats.lastProductionCaptureCopyGpuTiming.serial=gpu;Need(probe.Append(Frame(serial),stats),"Streaming valid fill");}
}
void StreamingTests(unsigned& negatives){
    using Probe=Mistwatch::ReplayCostProbe;
    static_assert(Probe::MaximumRows==8192&&Probe::MaximumChunks==64);
    const auto directory=Directory();Probe probe;Need(probe.BeginStreaming(directory),"Begin explicit streaming");
    Need(probe.Streaming()&&!probe.StreamFailed(),"Streaming selected");Fill(probe,1,Probe::MaximumRows);
    Need(probe.ChunkCount()==0&&probe.Rows().size()==Probe::MaximumRows,"No premature flush");
    const auto reject=[&](Aevrynd::EngineFrameCost3D frame,Aevrynd::RendererStats stats){
        const auto rows=probe.Rows();const auto total=probe.TotalRows();const auto chunks=probe.ChunkCount();
        Need(!probe.Append(frame,stats),"Streaming invalid row accepted");
        Need(probe.Rows()==rows&&probe.TotalRows()==total&&probe.ChunkCount()==chunks&&!probe.StreamFailed(),"Rejected streaming row changed evidence");
        Need(!std::filesystem::exists(ChunkPath(directory,chunks)),"Rejected streaming row wrote next chunk");++negatives;
    };
    const std::uint64_t next=Probe::MaximumRows+1;auto bad=Frame(next);bad.complete=false;reject(bad,Stats(next));
    reject(Frame(next+1),Stats(next+1));bad=Frame(next);bad.workNanoseconds=50;reject(bad,Stats(next));
    bad=Frame(next);bad.fixedUpdates=6;reject(bad,Stats(next));
    auto stats=Stats(next);stats.lastSubmissionTelemetryReceipt.serial=Probe::MaximumRows;reject(Frame(next),stats);
    stats=Stats(next);stats.lastCompletedGpuTimingReceipt.serial=next+1;reject(Frame(next),stats);
    stats=Stats(next);stats.lastProductionCaptureCopyGpuTiming.serial=next+1;reject(Frame(next),stats);
    stats=Stats(next);stats.lastFrameCapturePipelineCost={next+1,0,0,0,0,0,{},false};reject(Frame(next),stats);
    stats=Stats(next);stats.lastFrameCapturePipelineCost={next,30,0,0,0,20,{},false};reject(Frame(next),stats);
    stats=Stats(next);stats.lastFrameCapturePipelineCost={next,0,0,0,0,20,{0,0,0,0,0,false},true};reject(Frame(next),stats);
    stats=Stats(next);stats.lastPresentObservation.observed=true;stats.lastPresentObservation.serial=next-1;reject(Frame(next),stats);
    stats.lastPresentObservation.serial=next;stats.lastPresentObservation.clockValid=true;reject(Frame(next),stats);
    stats=Stats(next);stats.lastCompletedGpuTimingReceipt.serial=2;stats.lastProductionCaptureCopyGpuTiming.serial=2;
    Need(probe.Append(Frame(next),stats)&&probe.ChunkCount()==1&&probe.Rows().size()==1,"Validated boundary row flushes full first chunk");
    Need(probe.Rows().front()[14]==2&&probe.Rows().front()[16]==2,"Boundary new GPU serial retained");
    Need(probe.Append(Frame(next),stats)&&probe.Rows().size()==1&&probe.ChunkCount()==1,"Boundary same frame deduplicated");
    Fill(probe,next+1,2*Probe::MaximumRows,2);
    stats=Stats(2*Probe::MaximumRows+1);reject(Frame(2*Probe::MaximumRows+1),stats);
    stats.lastCompletedGpuTimingReceipt.serial=2;reject(Frame(2*Probe::MaximumRows+1),stats);
    Fill(probe,2*Probe::MaximumRows+1,2*Probe::MaximumRows+3,3);
    Need(probe.TotalRows()==2*Probe::MaximumRows+3&&probe.ChunkCount()==2,"Multiple full chunks and bounded tail");
    Need(!probe.Write(directory/"missing"),"Output directory must equal stream identity");++negatives;
    Need(probe.Write(directory)&&probe.ChunkCount()==3,"Streaming final assembly");
    std::ifstream data(directory/"frame-cost.tsv");std::string line;Need(bool(std::getline(data,line)),"Streaming TSV header");
    std::uint64_t serial{};
    while(std::getline(data,line)){
        Probe::Row row{};std::istringstream fields(line);for(auto& field:row)Need(bool(fields>>field),"All 67 streaming fields");
        std::string extra;Need(!(fields>>extra),"No unexpected streaming field");++serial;
        Need(row[0]==serial&&row[1]==serial,"Streaming serials continuous without loss");
        const auto expectedGpu=serial==1?1:serial==next?2:serial==2*Probe::MaximumRows+1?3:0;
        Need(row[14]==static_cast<std::uint64_t>(expectedGpu)&&row[16]==static_cast<std::uint64_t>(expectedGpu),"GPU and copy deduplicated across chunks");
    }
    Need(data.eof()&&serial==2*Probe::MaximumRows+3,"Complete streaming TSV count");data.close();
    std::ifstream meta(directory/"frame-cost.txt");const std::string receipt{std::istreambuf_iterator<char>(meta),{}};meta.close();
    Need(receipt.find("schema=mistwatch.replay-frame-cost.3\n")!=std::string::npos&&receipt.find("capacity=8192\nchunk_capacity=64\nchunks=3\nmaximum_total_rows=524288\noverflow=0\n")!=std::string::npos,"Explicit bounded schema 3 metadata");
    Need(receipt.find("io_scope=chunk-write-in-append-final-assembly-on-stop\nchunk_io_frame_attribution=caller-frame-may-include-io\nchunk_io_separately_measured=0\n")!=std::string::npos,"Streaming IO scope disclosed without false exclusion");
    Need(!probe.Write(directory)&&!probe.Append(Frame(serial+1),Stats(serial+1)),"Finalized stream cannot be reused");++negatives;Clean(directory,3);

    Probe invalid;Need(!invalid.BeginStreaming("relative")&&!invalid.BeginStreaming(directory),"Relative and absent directories rejected");negatives+=2;
    const auto markerDirectory=Directory();{std::ofstream marker(markerDirectory/"frame-cost.stream");marker<<"existing";}
    Need(!invalid.BeginStreaming(markerDirectory/L"..")&&!invalid.BeginStreaming(markerDirectory/L"frame-cost.stream"),"Traversal and file output rejected");negatives+=2;
    Probe existing;Need(!existing.BeginStreaming(markerDirectory),"Existing stream marker not overwritten");++negatives;Clean(markerDirectory,0);
    const auto outputDirectory=Directory();Probe output;Need(output.BeginStreaming(outputDirectory),"Output conflict begin");Fill(output,1,1);
    {std::ofstream prior(outputDirectory/"frame-cost.tsv");prior<<"do-not-overwrite";}
    Need(!output.Write(outputDirectory)&&output.StreamFailed()&&output.ChunkCount()==0,"Existing final output refuses without chunk side effect");++negatives;
    {std::ifstream prior(outputDirectory/"frame-cost.tsv");std::string value;std::getline(prior,value);Need(value=="do-not-overwrite","Prior final output retained");}Clean(outputDirectory,0);
    const auto metaDirectory=Directory();Probe priorMeta;Need(priorMeta.BeginStreaming(metaDirectory),"Meta conflict begin");Fill(priorMeta,1,1);
    {std::ofstream prior(metaDirectory/"frame-cost.txt");prior<<"original-meta";}
    Need(!priorMeta.Write(metaDirectory)&&priorMeta.StreamFailed()&&!std::filesystem::exists(metaDirectory/"frame-cost.tsv"),"Existing metadata refuses before assembly");++negatives;Clean(metaDirectory,0);

    const auto blockedDirectory=Directory();Probe blocked;Need(blocked.BeginStreaming(blockedDirectory),"IO conflict begin");Fill(blocked,1,Probe::MaximumRows);
    {std::ofstream conflict(ChunkPath(blockedDirectory,0));conflict<<"do-not-overwrite";}
    Need(!blocked.Append(Frame(next),Stats(next))&&blocked.StreamFailed()&&blocked.Rows().size()==Probe::MaximumRows&&blocked.TotalRows()==Probe::MaximumRows,"Chunk IO failure preserves buffer and serial");
    Need(!blocked.Append(Frame(next),Stats(next))&&!blocked.Write(blockedDirectory),"Chunk IO failure stays closed");++negatives;Clean(blockedDirectory,1);

    const auto replacedDirectory=Directory();Probe replaced;Need(replaced.BeginStreaming(replacedDirectory),"Directory identity begin");Fill(replaced,1,next);
    const auto keptDirectory=std::filesystem::path(replacedDirectory.native()+L"-preserved");
    std::filesystem::rename(replacedDirectory,keptDirectory);Need(std::filesystem::create_directory(replacedDirectory),"Replacement directory created");
    Need(std::filesystem::copy_file(keptDirectory/"frame-cost.stream",replacedDirectory/"frame-cost.stream"),"Copied marker fixture");
    Need(!replaced.Write(replacedDirectory)&&replaced.StreamFailed()&&!std::filesystem::exists(replacedDirectory/"frame-cost.tsv"),"Same-path directory shadow rejected");++negatives;
    Clean(replacedDirectory,0);Clean(keptDirectory,1);

    for(unsigned mode=0;mode<5;++mode){
        const auto damagedDirectory=Directory();Probe damaged;Need(damaged.BeginStreaming(damagedDirectory),"Damaged stream begin");Fill(damaged,1,next);
        const auto chunk=ChunkPath(damagedDirectory,0);
        if(mode==0){std::fstream file(chunk,std::ios::in|std::ios::out|std::ios::binary);file.seekp(11*sizeof(std::uint64_t)+5*sizeof(std::uint64_t));const std::uint64_t value=900;file.write(reinterpret_cast<const char*>(&value),sizeof(value));Need(bool(file),"Corruption fixture written");}
        else if(mode==1)std::filesystem::resize_file(chunk,std::filesystem::file_size(chunk)-1);
        else if(mode==2){std::fstream file(chunk,std::ios::in|std::ios::out|std::ios::binary);file.seekp(2*sizeof(std::uint64_t));const std::uint64_t value=0;file.write(reinterpret_cast<const char*>(&value),sizeof(value));Need(bool(file),"Identity fixture written");}
        else if(mode==3){std::filesystem::rename(chunk,damagedDirectory/"kept.bin");Need(std::filesystem::copy_file(damagedDirectory/"kept.bin",chunk),"Same-name shadow fixture copied");}
        else {std::ofstream file(damagedDirectory/"frame-cost.stream",std::ios::binary|std::ios::trunc);file<<"invalid-marker";Need(bool(file),"Marker fixture written");}
        Need(!damaged.Write(damagedDirectory)&&damaged.StreamFailed()&&!std::filesystem::exists(damagedDirectory/"frame-cost.tsv")&&!std::filesystem::exists(damagedDirectory/"frame-cost.txt"),"Corrupted stream cannot assemble success");++negatives;Clean(damagedDirectory,2);
    }
    // 即使每段都有效，也不能無界增加段數與身分表。
    const auto boundedDirectory=Directory();Probe bounded;Need(bounded.BeginStreaming(boundedDirectory),"Capacity boundary begin");
    Fill(bounded,1,Probe::MaximumRows*Probe::MaximumChunks);
    const auto capacity=Probe::MaximumRows*Probe::MaximumChunks;
    Need(bounded.ChunkCount()==Probe::MaximumChunks-1&&bounded.Rows().size()==Probe::MaximumRows,"Fixed chunk metadata boundary");
    Need(!bounded.Append(Frame(capacity+1),Stats(capacity+1))&&bounded.Overflowed()&&!bounded.StreamFailed()&&bounded.TotalRows()==capacity&&!std::filesystem::exists(ChunkPath(boundedDirectory,Probe::MaximumChunks-1)),"Capacity exhaustion does not drop or write new rows");++negatives;
    Clean(boundedDirectory,Probe::MaximumChunks-1);
}
}
int main(){
    try{
        Mistwatch::ReplayCostProbe probe;probe.Begin();auto stats=Stats(1);
        stats.lastFrameCapturePipelineCost={1,1,2,3,4,25,{1,2,3,4,11,true},true};
        Need(probe.Append(Frame(1),stats),"Initial real serial");
        Need(probe.Rows().size()==1&&probe.Rows()[0][18]==1&&probe.Rows()[0][29]==1,"Capture attached to actual submission");
        Need(probe.Append(Frame(1),stats)&&probe.Rows().size()==1,"Same completed frame not duplicated on multiple updates");
        stats.lastSubmissionTelemetryReceipt.serial=2;
        Need(probe.Append(Frame(2),stats),"Next frame");
        Need(probe.Rows()[1][14]==0&&probe.Rows()[1][16]==0&&probe.Rows()[1][18]==0,"Stale GPU/capture samples not reused");
        stats.lastSubmissionTelemetryReceipt.serial=3;
        stats.lastCompletedGpuTimingReceipt.serial=2;stats.lastProductionCaptureCopyGpuTiming.serial=2;
        Need(probe.Append(Frame(3),stats)&&probe.Rows()[2][14]==2&&probe.Rows()[2][16]==2,"New completed GPU serial recorded once");
        unsigned negatives{};
        const auto reject=[&](Aevrynd::EngineFrameCost3D frame,Aevrynd::RendererStats input){
            const auto count=probe.Rows().size();Need(!probe.Append(frame,input),"Invalid cost observation accepted");
            Need(probe.Rows().size()==count,"Rejection changed rows");++negatives;
        };
        auto bad=Frame(4);bad.complete=false;reject(bad,Stats(4));
        bad=Frame(4);bad.serial=0;reject(bad,Stats(4));
        reject(Frame(5),Stats(5));
        bad=Frame(4);bad.fixedUpdates=6;reject(bad,Stats(4));
        bad=Frame(4);bad.workNanoseconds=50;reject(bad,Stats(4));
        bad=Frame(4);bad.fixedUpdateNanoseconds=(std::numeric_limits<std::uint64_t>::max)();reject(bad,Stats(4));
        stats=Stats(4);stats.lastSubmissionTelemetryReceipt.serial=3;reject(Frame(4),stats);
        stats=Stats(4);stats.lastCompletedGpuTimingReceipt.serial=5;reject(Frame(4),stats);
        stats=Stats(4);stats.lastCompletedGpuTimingReceipt.serial=1;reject(Frame(4),stats);
        stats=Stats(4);stats.lastCompletedGpuTimingReceipt.serial=2;stats.lastProductionCaptureCopyGpuTiming.serial=5;reject(Frame(4),stats);
        stats=Stats(4);stats.lastCompletedGpuTimingReceipt.serial=2;stats.lastProductionCaptureCopyGpuTiming.serial=2;
        stats.lastFrameCapturePipelineCost={4,30,0,0,0,20,{},false};reject(Frame(4),stats);
        stats.lastFrameCapturePipelineCost={4,0,0,0,0,20,{30,0,0,0,20,true},true};reject(Frame(4),stats);
        stats.lastFrameCapturePipelineCost={4,0,0,0,0,20,{0,0,0,0,0,false},true};reject(Frame(4),stats);
        stats.lastFrameCapturePipelineCost={5,0,0,0,0,0,{},false};reject(Frame(4),stats);
        stats.lastFrameCapturePipelineCost={4,0,0,0,0,41,{},false};reject(Frame(4),stats);
        stats=Stats(4);stats.lastCompletedGpuTimingReceipt.serial=2;stats.lastProductionCaptureCopyGpuTiming.serial=2;
        auto& present=stats.lastPresentObservation;present.observed=true;present.serial=3;reject(Frame(4),stats);
        present.serial=4;present.captureRequested=true;reject(Frame(4),stats);
        present.captureRequested=false;present.clockValid=true;present.qpcBegin=2;present.qpcEnd=3;reject(Frame(4),stats);
        present.qpcFrequency=100;present.qpcEnd=1;reject(Frame(4),stats);
        present.qpcEnd=3;present.descriptorValid=true;present.bufferCount=2;present.height=720;reject(Frame(4),stats);
        present.width=1280;present.bufferCount=1;reject(Frame(4),stats);
        present.bufferCount=2;present.syncInterval=1;present.swapEffect=4;present.before.valid=present.after.valid=true;
        present.before.visible=false;present.after.visible=true;present.before.cloakedHresult=0x80070057u;
        Need(probe.Append(Frame(4),stats),"Valid observed Present attached to same serial");
        const auto& observed=probe.Rows().back();
        Need(observed[31]==1&&observed[32]==4&&observed[37]==0&&observed[50]==0&&observed[59]==1&&
            observed[55]==0x80070057u&&observed[56]==0,"Raw unavailable cloak result and natural window change retained");
        // S_OK 不會自動產生可見／未遮蔽判定，未知欄位保持未知。
        Aevrynd::DrawList list;Need(!list.PresentObservationEnabled3D(),"Production observation defaults off");
        list.SetPresentObservationEnabled3D(true);list.Clear();Need(!list.PresentObservationEnabled3D(),"Observation reset with draw list");
        // 用無 GPU/擷取資料填滿固定上限，不能無界延長記錄。
        stats={};
        for(std::uint64_t serial=5;serial<=Mistwatch::ReplayCostProbe::MaximumRows;++serial){
            stats.lastSubmissionTelemetryReceipt.serial=serial;Need(probe.Append(Frame(serial),stats),"Finite row fill");
        }
        stats.lastSubmissionTelemetryReceipt.serial=Mistwatch::ReplayCostProbe::MaximumRows+1;
        reject(Frame(Mistwatch::ReplayCostProbe::MaximumRows+1),stats);
        Need(probe.Overflowed()&&probe.Rows().size()==Mistwatch::ReplayCostProbe::MaximumRows,"Explicit bounded overflow");
        GUID guid{};Need(SUCCEEDED(CoCreateGuid(&guid)),"Unique output identity");
        std::array<wchar_t,40> text{};Need(StringFromGUID2(guid,text.data(),int(text.size()))>0,"Unique output name");
        const auto directory=std::filesystem::temp_directory_path()/(std::wstring(L"AevryndReplayCost-")+text.data());
        Need(std::filesystem::create_directory(directory),"Exclusive output directory");
        Need(probe.Write(directory,true),"Overflow preserves previous evidence and failure state");
        std::ifstream data(directory/"frame-cost.tsv");std::string line;std::size_t lines{};
        while(std::getline(data,line))++lines;
        Need(data.eof()&&lines==Mistwatch::ReplayCostProbe::MaximumRows+1,"All bounded rows exported");
        data.close();
        std::ifstream metadata(directory/"frame-cost.txt");
        const std::string content{std::istreambuf_iterator<char>(metadata),{}};
        Need(content.find("overflow=1\nobservation_rejected=1\n")!=std::string::npos,"Overflow is not a success receipt");
        metadata.close();
        // 只刪除本次 GUID 目錄中兩個明列的檔案。
        std::filesystem::remove(directory/"frame-cost.tsv");std::filesystem::remove(directory/"frame-cost.txt");std::filesystem::remove(directory);
        StreamingTests(negatives);
        std::cout<<"REPLAY_COST_CPU_PASS columns="<<Mistwatch::ReplayCostProbe::Columns.size()<<" rows="<<probe.Rows().size()<<" negatives="<<negatives<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<"REPLAY_COST_CPU_FAIL "<<error.what()<<'\n';return 1;}
}
