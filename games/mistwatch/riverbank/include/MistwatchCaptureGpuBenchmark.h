#pragma once
#include <Aevrynd/Renderer.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Mistwatch {
// 每個渲染迴圈觀測完成收據；固定更新可為零，不能以更新頻率決定採樣。
class CaptureGpuBenchmark {
public:
    static constexpr std::uint64_t WarmupFrames=60;
    static constexpr std::size_t RequiredSamples=120;
    bool Observe(const Aevrynd::RendererStats::CompletedGpuTimingReceipt3D& receipt,std::uint64_t submittedSerial) noexcept {
        if(!receipt.Complete()||receipt.serial<=WarmupFrames||receipt.serial<=lastSerial_||
           receipt.serial>submittedSerial||count_==RequiredSamples)return false;
        milliseconds_[count_]=double(receipt.nanoseconds)/1000000.0;
        receipts_[count_]=receipt;
        serials_[count_++]=receipt.serial;lastSerial_=receipt.serial;
        return true;
    }
    [[nodiscard]] bool Complete()const noexcept{return count_==RequiredSamples;}
    [[nodiscard]] std::size_t Count()const noexcept{return count_;}
    [[nodiscard]] std::span<const double> Milliseconds()const noexcept{return {milliseconds_.data(),count_};}
    [[nodiscard]] std::span<const std::uint64_t> Serials()const noexcept{return {serials_.data(),count_};}
    [[nodiscard]] std::span<const Aevrynd::RendererStats::CompletedGpuTimingReceipt3D> Receipts()const noexcept{return {receipts_.data(),count_};}
private:
    std::array<double,RequiredSamples> milliseconds_{};
    std::array<std::uint64_t,RequiredSamples> serials_{};
    std::array<Aevrynd::RendererStats::CompletedGpuTimingReceipt3D,RequiredSamples> receipts_{};
    std::size_t count_{};
    std::uint64_t lastSerial_{};
};
}
