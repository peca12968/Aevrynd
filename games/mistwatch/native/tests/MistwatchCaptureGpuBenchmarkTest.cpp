#include "MistwatchCaptureGpuBenchmark.h"
#include <iostream>
#include <stdexcept>

namespace {
void Need(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
auto Receipt(std::uint64_t serial){return Aevrynd::RendererStats::CompletedGpuTimingReceipt3D{serial,10,20,100,100000000,123};}
}
int main(){
    try{
        Mistwatch::CaptureGpuBenchmark renderObserver,updateObserver;
        // 模擬同一批完整GPU收據：每十個渲染有一輪零更新，其餘可有五次更新。
        for(std::uint64_t serial=1;serial<=180;++serial){
            const auto receipt=Receipt(serial);
            renderObserver.Observe(receipt,serial+3);
            const unsigned updates=serial%10==0?0:5;
            for(unsigned update=0;update<updates;++update)updateObserver.Observe(receipt,serial+3);
        }
        Need(renderObserver.Complete()&&renderObserver.Count()==120,"Every rendered receipt must be collected despite zero updates");
        Need(updateObserver.Count()==108&&!updateObserver.Complete(),"Legacy fixed-update observation loses twelve complete samples");
        for(std::size_t i=0;i<120;++i){
            Need(renderObserver.Serials()[i]==i+61,"Warmup excluded and completed serial order retained");
            Need(renderObserver.Milliseconds()[i]==100.,"Raw GPU duration converted once");
            const auto raw=renderObserver.Receipts()[i];
            Need(raw.serial==i+61&&raw.beginTick==10&&raw.endTick==20&&raw.frequency==100&&
                 raw.nanoseconds==100000000&&raw.bloomResourceIdentity==123,"Raw timestamp and original resource identity retained");
        }
        Need(!renderObserver.Observe(Receipt(181),184)&&renderObserver.Count()==120,"Bounded sample capacity cannot grow");
        Mistwatch::CaptureGpuBenchmark guarded;
        unsigned negatives{};
        const auto reject=[&](auto receipt,std::uint64_t submitted){
            Need(!guarded.Observe(receipt,submitted)&&guarded.Count()==0,"Invalid or warmup sample accepted");++negatives;
        };
        reject(Receipt(60),63);reject(Receipt(61),60);
        auto bad=Receipt(61);bad.serial=0;reject(bad,64);
        bad=Receipt(61);bad.endTick=9;reject(bad,64);
        bad=Receipt(61);bad.frequency=0;reject(bad,64);
        bad=Receipt(61);bad.nanoseconds=0;reject(bad,64);
        bad=Receipt(61);bad.bloomResourceIdentity=0;reject(bad,64);
        Need(guarded.Observe(Receipt(61),64),"First complete post-warmup receipt accepted");
        Need(!guarded.Observe(Receipt(61),65)&&!guarded.Observe(Receipt(60),65)&&guarded.Count()==1,"Duplicates and stale serials do not count twice");
        Need(guarded.Observe(Receipt(63),66)&&guarded.Serials()[1]==63,"Missing serial is never fabricated");
        Need(!guarded.Observe(Receipt(62),66)&&guarded.Count()==2,"Post-warmup out-of-order receipt rejected");
        bad=Receipt(100);bad.frequency=0;
        Need(!guarded.Observe(bad,103)&&guarded.Count()==2,"Invalid high serial cannot advance observer");
        Need(guarded.Observe(Receipt(64),67)&&guarded.Serials()[2]==64,"Valid lower serial accepted after invalid high serial");
        std::cout<<"CAPTURE_GPU_BENCHMARK_CPU_PASS samples=120 legacy_samples=108 negatives="<<negatives<<'\n';
        return 0;
    }catch(const std::exception& error){std::cerr<<"CAPTURE_GPU_BENCHMARK_CPU_FAIL "<<error.what()<<'\n';return 1;}
}
