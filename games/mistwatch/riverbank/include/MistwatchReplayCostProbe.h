#pragma once
#include <Aevrynd/Engine.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

namespace Mistwatch {
// 預設維持結束時旁錄；明確選入分段模式時仍保留固定容量及完整資料。
class ReplayCostProbe {
public:
    static constexpr std::size_t MaximumRows=8192;
    static constexpr std::size_t MaximumChunks=64;
    static constexpr std::array<std::string_view,67> Columns{
        "engine_serial","submission_serial","updates","update_ns","draw_prepare_ns","game_render_ns","renderer_ns","post_render_ns","work_ns",
        "render_prepare_ns","render_record_ns","queue_ns","frame_slot_wait_ns","present_ns",
        "gpu_serial","gpu_ns","gpu_copy_serial","gpu_copy_ns",
        "capture_serial","capture_fence_wait_ns","capture_map_ns","capture_analysis_ns","png_setup_ns","png_encode_write_ns",
        "png_publish_ns","png_verify_hash_ns","png_total_ns","capture_unmap_ns","capture_total_ns","capture_complete","png_complete",
        "present_observed","present_serial","present_qpc_begin","present_qpc_end","present_qpc_hz","present_clock_valid",
        "present_hr","present_sync_interval","present_flags","present_thread_cycles","present_cycles_valid","present_capture_requested",
        "present_desc_valid","swap_effect","swap_flags","buffer_count","output_width","output_height",
        "before_window_valid","before_visible","before_iconic","before_foreground","before_placement_valid","before_show_cmd",
        "before_cloak_hr","before_cloak_valid","before_cloaked",
        "after_window_valid","after_visible","after_iconic","after_foreground","after_placement_valid","after_show_cmd",
        "after_cloak_hr","after_cloak_valid","after_cloaked"};
    using Row=std::array<std::uint64_t,Columns.size()>;
    static_assert(sizeof(Row)==67*sizeof(std::uint64_t)&&std::endian::native==std::endian::little);
    void Begin(){rows_.reserve(MaximumRows);}
    [[nodiscard]] bool BeginStreaming(const std::filesystem::path& directory,bool measureGpuStages=false){
        if(streaming_||streamFailed_||!rows_.empty()||lastEngineSerial_||!SafeDirectory(directory))return false;
        Handle directoryLock(CreateFileW(directory.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        if(!directoryLock.Valid()||!Identity(directoryLock.Get(),directoryIdentity_))return false;
        const auto marker=directory/L"frame-cost.stream";
        Handle file(CreateFileW(marker.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!file.Valid())return false;
        LARGE_INTEGER clock{};if(!QueryPerformanceCounter(&clock))return Fail();
        streamIdentity_=HashBytes(directory.native().data(),directory.native().size()*sizeof(wchar_t));
        streamIdentity_^=static_cast<std::uint64_t>(clock.QuadPart)^static_cast<std::uint64_t>(GetCurrentProcessId());
        if(!streamIdentity_)streamIdentity_=1;
        const std::array<std::uint64_t,3> markerData{MarkerMagic,3,streamIdentity_};
        if(!WriteBytes(file.Get(),markerData.data(),sizeof(markerData))||!FlushFileBuffers(file.Get())||!Identity(file.Get(),markerIdentity_)){
            streamFailed_=true;return false;
        }
        streamDirectory_=directory;rows_.reserve(MaximumRows);streaming_=true;
        measureGpuStages_=measureGpuStages;
        if(measureGpuStages_)gpuStageRows_.reserve(MaximumRows);
        return true;
    }
    [[nodiscard]] bool Append(const Aevrynd::EngineFrameCost3D& frame,const Aevrynd::RendererStats& stats){
        if(streamFailed_||finalized_)return false;
        if(!frame.complete||!frame.serial||!stats.lastSubmissionTelemetryReceipt.serial||frame.fixedUpdates>5)return false;
        if(frame.serial==lastEngineSerial_)return true;
        if(frame.serial!=lastEngineSerial_+1||stats.lastSubmissionTelemetryReceipt.serial<=lastSubmissionSerial_)return false;
        std::uint64_t remaining=frame.workNanoseconds;
        for(const auto value:{frame.fixedUpdateNanoseconds,frame.drawPreparationNanoseconds,frame.gameRenderNanoseconds,frame.rendererNanoseconds,frame.postRenderNanoseconds}){
            if(value>remaining)return false;remaining-=value;
        }
        if(remaining)return false;
        const auto& gpu=stats.lastCompletedGpuTimingReceipt;
        if(gpu.Complete()&&(gpu.serial<lastGpuSerial_||gpu.serial>stats.lastSubmissionTelemetryReceipt.serial))return false;
        const bool newGpu=gpu.Complete()&&gpu.serial>lastGpuSerial_;
        const auto& scene=stats.lastCompletedGpuSceneTiming;
        if(newGpu&&measureGpuStages_){
            const auto& boundaries=scene.boundaries;
            if(scene.serial!=gpu.serial||boundaries.front()<gpu.beginTick||boundaries.back()>gpu.endTick||
                !std::is_sorted(boundaries.begin(),boundaries.end()))return false;
            if(gpuStageRows_.size()>=MaximumRows*MaximumChunks){overflow_=true;return false;}
        }
        const auto& copy=stats.lastProductionCaptureCopyGpuTiming;
        if(copy.valid&&(copy.serial<lastCopySerial_||copy.serial>stats.lastSubmissionTelemetryReceipt.serial))return false;
        const bool newCopy=copy.valid&&copy.recorded&&copy.serial>lastCopySerial_;
        const auto& capture=stats.lastFrameCapturePipelineCost;
        if(capture.submissionSerial>stats.lastSubmissionTelemetryReceipt.serial)return false;
        const bool captured=capture.submissionSerial==stats.lastSubmissionTelemetryReceipt.serial;
        const auto& present=stats.lastPresentObservation;
        if(present.observed){
            if(present.serial!=stats.lastSubmissionTelemetryReceipt.serial||present.captureRequested!=captured)return false;
            if(present.clockValid&&(!present.qpcFrequency||present.qpcEnd<present.qpcBegin))return false;
            if(present.descriptorValid&&(!present.width||!present.height||present.bufferCount<2))return false;
        }
        Row row{frame.serial,stats.lastSubmissionTelemetryReceipt.serial,frame.fixedUpdates,frame.fixedUpdateNanoseconds,
            frame.drawPreparationNanoseconds,frame.gameRenderNanoseconds,frame.rendererNanoseconds,frame.postRenderNanoseconds,frame.workNanoseconds,
            stats.lastRenderPreparationCpuNanoseconds,stats.lastRenderCommandRecordingCpuNanoseconds,stats.lastRenderQueueExecuteCpuNanoseconds,
            stats.lastRenderFenceWaitCpuNanoseconds,stats.lastRenderPresentCpuNanoseconds,
            newGpu?gpu.serial:0,newGpu?gpu.nanoseconds:0,newCopy?copy.serial:0,newCopy?copy.nanoseconds:0};
        if(captured){
            if(capture.totalNanoseconds>frame.rendererNanoseconds)return false;
            std::uint64_t left=capture.totalNanoseconds;
            for(const auto value:{capture.fenceWaitNanoseconds,capture.mapNanoseconds,capture.analysisNanoseconds,capture.png.totalNanoseconds,capture.unmapNanoseconds}){
                if(value>left)return false;left-=value;
            }
            left=capture.png.totalNanoseconds;
            for(const auto value:{capture.png.setupNanoseconds,capture.png.encodeWriteNanoseconds,capture.png.publishNanoseconds,capture.png.verifyHashNanoseconds}){
                if(value>left)return false;left-=value;
            }
            if(capture.complete&&!capture.png.complete)return false;
            row[18]=capture.submissionSerial;row[19]=capture.fenceWaitNanoseconds;row[20]=capture.mapNanoseconds;row[21]=capture.analysisNanoseconds;
            row[22]=capture.png.setupNanoseconds;row[23]=capture.png.encodeWriteNanoseconds;row[24]=capture.png.publishNanoseconds;
            row[25]=capture.png.verifyHashNanoseconds;row[26]=capture.png.totalNanoseconds;row[27]=capture.unmapNanoseconds;
            row[28]=capture.totalNanoseconds;row[29]=capture.complete;row[30]=capture.png.complete;
        }
        if(present.observed){
            const auto& before=present.before;const auto& after=present.after;
            const std::array<std::uint64_t,36> observation{present.observed,present.serial,present.qpcBegin,present.qpcEnd,present.qpcFrequency,present.clockValid,
                present.hresult,present.syncInterval,present.flags,present.threadCpuCycles,present.threadCpuCyclesValid,present.captureRequested,
                present.descriptorValid,present.swapEffect,present.swapFlags,present.bufferCount,present.width,present.height,
                before.valid,before.visible,before.iconic,before.foreground,before.placementValid,before.showCommand,before.cloakedHresult,before.cloakedValid,before.cloaked,
                after.valid,after.visible,after.iconic,after.foreground,after.placementValid,after.showCommand,after.cloakedHresult,after.cloakedValid,after.cloaked};
            std::copy(observation.begin(),observation.end(),row.begin()+31);
        }
        // 新列全部驗證後才寫出上一個滿段；拒絕的列不會產生磁碟副作用。
        if(rows_.size()==MaximumRows){
            if(!streaming_||chunkCount_+1>=MaximumChunks){overflow_=true;return false;}
            if(!FlushChunk())return false;
        }
        rows_.push_back(row);lastEngineSerial_=frame.serial;lastSubmissionSerial_=stats.lastSubmissionTelemetryReceipt.serial;
        if(newGpu){
            lastGpuSerial_=gpu.serial;
            if(measureGpuStages_){
                GpuStageRow stage{gpu.serial,gpu.beginTick,gpu.endTick,gpu.frequency};
                std::copy(scene.boundaries.begin(),scene.boundaries.end(),stage.begin()+4);
                gpuStageRows_.push_back(stage);
            }
        }
        if(copy.valid)lastCopySerial_=copy.serial;
        return true;
    }
    [[nodiscard]] bool Write(const std::filesystem::path& directory,bool observationRejected=false){
        if(streamFailed_)return false;
        if(streaming_)return WriteStreaming(directory,observationRejected);
        std::ofstream out(directory/"frame-cost.tsv");
        for(std::size_t i=0;i<Columns.size();++i)out<<(i?"\t":"")<<Columns[i];out<<'\n';
        for(const auto& row:rows_){for(std::size_t i=0;i<row.size();++i)out<<(i?"\t":"")<<row[i];out<<'\n';}
        out.close();if(!out)return false;
        std::ofstream meta(directory/"frame-cost.txt");
        meta<<"schema=mistwatch.replay-frame-cost.2\nrows="<<rows_.size()<<"\ncapacity="<<MaximumRows<<"\noverflow="<<overflow_
            <<"\nobservation_rejected="<<observationRejected
            <<"\nwrite_timing=on-stop-only\ncpu_scope=fixed-update-through-post-render-excludes-input-message-pump"
            <<"\ngpu_scope=completed-serial-deduplicated-not-added-to-cpu\ninstrumentation_overhead_included=1"
            <<"\npresentation_window_state=before-and-after-original-present\ncomplete_occlusion=unknown\nforeground_control=none\n";
        meta.close();return bool(meta);
    }
    [[nodiscard]] const std::vector<Row>& Rows()const noexcept{return rows_;}
    [[nodiscard]] bool Overflowed()const noexcept{return overflow_;}
    [[nodiscard]] bool Streaming()const noexcept{return streaming_;}
    [[nodiscard]] bool StreamFailed()const noexcept{return streamFailed_;}
    [[nodiscard]] std::size_t ChunkCount()const noexcept{return chunkCount_;}
    [[nodiscard]] std::uint64_t TotalRows()const noexcept{return flushedRows_+rows_.size();}
private:
    // 分段檔及身分表都有固定上限；不把完整回放重新載回記憶體。
    static constexpr std::uint64_t ChunkMagic=0x3354534f4356574dull,MarkerMagic=0x334d52545356574dull;
    class Handle {
    public:
        explicit Handle(HANDLE value=INVALID_HANDLE_VALUE):value_(value){}
        ~Handle(){if(Valid())CloseHandle(value_);}
        Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
        [[nodiscard]] bool Valid()const{return value_!=INVALID_HANDLE_VALUE&&value_!=nullptr;}
        [[nodiscard]] HANDLE Get()const{return value_;}
    private:HANDLE value_;
    };
    struct FileIdentity {DWORD volume{},high{},low{};};
    struct Chunk {std::uint64_t rows{},checksum{},firstEngine{},lastEngine{},firstSubmission{},lastSubmission{};FileIdentity file{};};
    static bool Identity(HANDLE file,FileIdentity& identity){
        BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(file,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
        identity={info.dwVolumeSerialNumber,info.nFileIndexHigh,info.nFileIndexLow};return true;
    }
    static bool SameIdentity(const FileIdentity& a,const FileIdentity& b){return a.volume==b.volume&&a.high==b.high&&a.low==b.low;}
    static bool SafeDirectory(const std::filesystem::path& directory){
        if(!directory.is_absolute()||directory!=directory.lexically_normal()||directory.native().size()>MAX_PATH-48)return false;
        const auto root=directory.root_name().native();
        if(root.size()!=2||root[1]!=L':'||!((root[0]>=L'A'&&root[0]<=L'Z')||(root[0]>=L'a'&&root[0]<=L'z')))return false;
        auto path=directory.root_path();
        const auto safe=[](const std::filesystem::path& input){const DWORD attr=GetFileAttributesW(input.c_str());return attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_DIRECTORY)&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT);};
        if(!safe(path))return false;
        for(const auto& component:directory.relative_path()){
            const auto name=component.native();if(name.empty()||name==L"."||name==L".."||name.back()==L'.'||name.back()==L' '||name.find(L':')!=std::wstring::npos)return false;
            path/=component;if(!safe(path))return false;
        }
        return true;
    }
    static std::uint64_t HashBytes(const void* data,std::size_t size,std::uint64_t hash=14695981039346656037ull){
        const auto* bytes=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<size;++i){hash^=bytes[i];hash*=1099511628211ull;}return hash;
    }
    static bool WriteBytes(HANDLE file,const void* data,std::size_t size){
        if(size>MAXDWORD)return false;DWORD written{};return WriteFile(file,data,static_cast<DWORD>(size),&written,nullptr)&&written==size;
    }
    static bool ReadBytes(HANDLE file,void* data,std::size_t size){
        if(size>MAXDWORD)return false;DWORD read{};return ReadFile(file,data,static_cast<DWORD>(size),&read,nullptr)&&read==size;
    }
    [[nodiscard]] std::filesystem::path ChunkPath(std::size_t index)const{
        std::array<wchar_t,48> name{};swprintf_s(name.data(),name.size(),L"frame-cost.chunk-%03u.bin",static_cast<unsigned>(index));return streamDirectory_/name.data();
    }
    bool Fail(){streamFailed_=true;return false;}
    bool FlushChunk(){
        if(rows_.empty())return true;
        if(chunkCount_>=MaximumChunks||!SafeDirectory(streamDirectory_))return Fail();
        Handle directoryLock(CreateFileW(streamDirectory_.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        FileIdentity currentDirectory{};
        if(!directoryLock.Valid()||!Identity(directoryLock.Get(),currentDirectory)||!SameIdentity(currentDirectory,directoryIdentity_))return Fail();
        Chunk chunk{rows_.size(),HashBytes(rows_.data(),rows_.size()*sizeof(Row)),rows_.front()[0],rows_.back()[0],rows_.front()[1],rows_.back()[1],{}};
        const std::array<std::uint64_t,11> header{ChunkMagic,3,streamIdentity_,chunkCount_,chunk.rows,flushedRows_,chunk.firstEngine,chunk.lastEngine,chunk.firstSubmission,chunk.lastSubmission,chunk.checksum};
        Handle file(CreateFileW(ChunkPath(chunkCount_).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!file.Valid()||!WriteBytes(file.Get(),header.data(),sizeof(header))||!WriteBytes(file.Get(),rows_.data(),rows_.size()*sizeof(Row))||!FlushFileBuffers(file.Get())||!Identity(file.Get(),chunk.file))return Fail();
        chunks_[chunkCount_++]=chunk;flushedRows_+=chunk.rows;rows_.clear();return true;
    }
    using GpuStageRow=std::array<std::uint64_t,12>;
    bool WriteGpuStages(const std::filesystem::path& directory){
        Handle out(CreateFileW((directory/L"gpu-stage-cost.tsv").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!out.Valid())return false;
        constexpr std::string_view header="gpu_serial\tbegin_tick\tend_tick\tfrequency\tshadow_begin\tshadow_end\tdepth_ao_begin\topaque_begin\tatmosphere_begin\ttransparency_post_begin\tcapture_begin\tcapture_end\n";
        if(!WriteBytes(out.Get(),header.data(),header.size()))return false;
        std::string line;line.reserve(12*22);
        for(const auto& row:gpuStageRows_){
            line.clear();
            for(const auto value:row){if(!line.empty())line+='\t';line+=std::to_string(value);}
            line+='\n';
            if(!WriteBytes(out.Get(),line.data(),line.size()))return false;
        }
        return FlushFileBuffers(out.Get())!=FALSE;
    }
    bool WriteStreaming(const std::filesystem::path& directory,bool rejected){
        if(finalized_||streamFailed_||directory!=streamDirectory_||!SafeDirectory(directory))return false;
        Handle directoryLock(CreateFileW(directory.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        FileIdentity currentDirectory{};
        if(!directoryLock.Valid()||!Identity(directoryLock.Get(),currentDirectory)||!SameIdentity(currentDirectory,directoryIdentity_))return Fail();
        // 不覆寫舊輸出；先核對所有分段，再建立完整 TSV。
        if(GetFileAttributesW((directory/L"frame-cost.tsv").c_str())!=INVALID_FILE_ATTRIBUTES||GetFileAttributesW((directory/L"frame-cost.txt").c_str())!=INVALID_FILE_ATTRIBUTES)return Fail();
        if(!FlushChunk())return false;
        Handle marker(CreateFileW((directory/L"frame-cost.stream").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        FileIdentity markerId{};std::array<std::uint64_t,3> markerData{};LARGE_INTEGER markerSize{};
        if(!marker.Valid()||!Identity(marker.Get(),markerId)||!SameIdentity(markerId,markerIdentity_)||!GetFileSizeEx(marker.Get(),&markerSize)||markerSize.QuadPart!=static_cast<LONGLONG>(sizeof(markerData))||!ReadBytes(marker.Get(),markerData.data(),sizeof(markerData))||markerData!=std::array<std::uint64_t,3>{MarkerMagic,3,streamIdentity_})return Fail();
        std::array<HANDLE,MaximumChunks> files{};files.fill(INVALID_HANDLE_VALUE);
        struct CloseAll {std::array<HANDLE,MaximumChunks>& files;~CloseAll(){for(const auto file:files)if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}} close{files};
        Row row{};std::uint64_t expectedEngine=1,lastSubmission{},count{};
        for(std::size_t i=0;i<chunkCount_;++i){
            files[i]=CreateFileW(ChunkPath(i).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
            FileIdentity identity{};LARGE_INTEGER size{};std::array<std::uint64_t,11> header{};const auto& chunk=chunks_[i];
            const std::array<std::uint64_t,11> expected{ChunkMagic,3,streamIdentity_,i,chunk.rows,count,chunk.firstEngine,chunk.lastEngine,chunk.firstSubmission,chunk.lastSubmission,chunk.checksum};
            if(files[i]==INVALID_HANDLE_VALUE||!Identity(files[i],identity)||!SameIdentity(identity,chunk.file)||!GetFileSizeEx(files[i],&size)||size.QuadPart!=static_cast<LONGLONG>(sizeof(header)+chunk.rows*sizeof(Row))||!ReadBytes(files[i],header.data(),sizeof(header))||header!=expected)return Fail();
            auto hash=14695981039346656037ull;
            for(std::uint64_t j=0;j<chunk.rows;++j){if(!ReadBytes(files[i],row.data(),sizeof(row))||row[0]!=expectedEngine||row[1]<=lastSubmission)return Fail();hash=HashBytes(row.data(),sizeof(row),hash);++expectedEngine;lastSubmission=row[1];++count;}
            if(hash!=chunk.checksum)return Fail();
        }
        if(count!=flushedRows_||count>MaximumRows*MaximumChunks)return Fail();
        Handle out(CreateFileW((directory/L"frame-cost.tsv").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!out.Valid())return Fail();
        std::string line;line.reserve(Columns.size()*22);
        for(std::size_t i=0;i<Columns.size();++i){if(i)line+='\t';line+=Columns[i];}line+='\n';if(!WriteBytes(out.Get(),line.data(),line.size()))return Fail();
        for(std::size_t i=0;i<chunkCount_;++i){
            LARGE_INTEGER offset{};offset.QuadPart=11*sizeof(std::uint64_t);if(!SetFilePointerEx(files[i],offset,nullptr,FILE_BEGIN))return Fail();
            for(std::uint64_t j=0;j<chunks_[i].rows;++j){
                if(!ReadBytes(files[i],row.data(),sizeof(row)))return Fail();line.clear();
                for(std::size_t k=0;k<row.size();++k){if(k)line+='\t';line+=std::to_string(row[k]);}line+='\n';if(!WriteBytes(out.Get(),line.data(),line.size()))return Fail();
            }
        }
        if(!FlushFileBuffers(out.Get()))return Fail();
        Handle meta(CreateFileW((directory/L"frame-cost.txt").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!meta.Valid())return Fail();
        const std::string receipt="schema=mistwatch.replay-frame-cost.3\nrows="+std::to_string(count)+"\ntotal_rows="+std::to_string(count)+"\ncapacity="+std::to_string(MaximumRows)+"\nchunk_capacity="+std::to_string(MaximumChunks)+"\nchunks="+std::to_string(chunkCount_)+"\nmaximum_total_rows="+std::to_string(MaximumRows*MaximumChunks)+"\noverflow="+std::to_string(overflow_)+"\nobservation_rejected="+std::to_string(rejected)+
            "\nwrite_timing=bounded-chunks-and-on-stop-assembly\nio_scope=chunk-write-in-append-final-assembly-on-stop\nchunk_io_frame_attribution=caller-frame-may-include-io\nchunk_io_separately_measured=0\nchunk_format=fixed-u64-header-and-67-u64-rows\nchunk_checksum=fnv1a64-raw-rows\nchunk_identity=stream-and-file-id\nstream_identity="+std::to_string(streamIdentity_)+
            "\ncpu_scope=fixed-update-through-post-render-excludes-input-message-pump\ngpu_scope=completed-serial-deduplicated-not-added-to-cpu\ninstrumentation_overhead_included=1\npresentation_window_state=before-and-after-original-present\ncomplete_occlusion=unknown\nforeground_control=none\n";
        if(!WriteBytes(meta.Get(),receipt.data(),receipt.size())||!FlushFileBuffers(meta.Get()))return Fail();
        if(measureGpuStages_&&!WriteGpuStages(directory))return Fail();
        finalized_=true;return true;
    }
    std::vector<Row> rows_;
    std::vector<GpuStageRow> gpuStageRows_;
    bool measureGpuStages_{};
    std::uint64_t lastEngineSerial_{},lastSubmissionSerial_{},lastGpuSerial_{},lastCopySerial_{};
    bool overflow_{};
    bool streaming_{},streamFailed_{},finalized_{};
    std::filesystem::path streamDirectory_;
    std::uint64_t streamIdentity_{},flushedRows_{};
    FileIdentity markerIdentity_{},directoryIdentity_{};
    std::array<Chunk,MaximumChunks> chunks_{};
    std::size_t chunkCount_{};
};
}
