#include <Aevrynd/DevelopmentUltraQuality3D.h>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Aevrynd;
using Microsoft::WRL::ComPtr;

namespace {

bool GpuValidationRequested() {
  char value[8]{};
  return GetEnvironmentVariableA("AEVRYND_GPU_VALIDATION", value,
                                 static_cast<DWORD>(sizeof(value))) > 0 &&
         value[0] == '1';
}

ComPtr<IDXGIAdapter1> SelectHardwareAdapter() {
  ComPtr<IDXGIFactory6> factory;
  if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) {
    return {};
  }
  for (UINT index = 0;; ++index) {
    ComPtr<IDXGIAdapter1> adapter;
    if (factory->EnumAdapterByGpuPreference(
            index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)) == DXGI_ERROR_NOT_FOUND) {
      return {};
    }
    DXGI_ADAPTER_DESC1 description{};
    if (SUCCEEDED(adapter->GetDesc1(&description)) &&
        (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
      return adapter;
    }
  }
}

std::uint64_t SevereMessageCount(ID3D12InfoQueue &queue) {
  std::uint64_t result{};
  const auto count = queue.GetNumStoredMessagesAllowedByRetrievalFilter();
  for (UINT64 index = 0; index < count; ++index) {
    SIZE_T bytes{};
    if (FAILED(queue.GetMessage(index, nullptr, &bytes)) || bytes == 0) {
      ++result;
      continue;
    }
    std::vector<std::byte> storage(bytes);
    auto *message = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
    if (FAILED(queue.GetMessage(index, message, &bytes)) ||
        message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION ||
        message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
        message->Severity == D3D12_MESSAGE_SEVERITY_WARNING) {
      ++result;
    }
  }
  return result;
}

} // namespace

int main() {
  const bool gpuValidation = GpuValidationRequested();
  ComPtr<ID3D12Debug> debug;
  if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
    return 2;
  }
  debug->EnableDebugLayer();
  if (gpuValidation) {
    ComPtr<ID3D12Debug1> debug1;
    if (FAILED(debug.As(&debug1))) {
      return 3;
    }
    debug1->SetEnableGPUBasedValidation(TRUE);
  }
  const auto adapter = SelectHardwareAdapter();
  if (!adapter) {
    return 4;
  }
  ComPtr<ID3D12Device> device;
  if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                               IID_PPV_ARGS(&device)))) {
    return 5;
  }
  std::uint64_t validationMessages{};
  {
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (FAILED(device.As(&infoQueue))) {
      return 6;
    }
    infoQueue->ClearStoredMessages();
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateCommandQueue(&queueDescription,
                                          IID_PPV_ARGS(&queue))) ||
        FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               IID_PPV_ARGS(&allocator))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         allocator.Get(), nullptr,
                                         IID_PPV_ARGS(&commandList))) ||
        FAILED(commandList->Close()) ||
        FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                   IID_PPV_ARGS(&fence)))) {
      return 7;
    }
    ID3D12CommandList *lists[]{commandList.Get()};
    queue->ExecuteCommandLists(1, lists);
    if (FAILED(queue->Signal(fence.Get(), 1))) {
      return 8;
    }
    const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) {
      return 9;
    }
    const bool alreadyComplete = fence->GetCompletedValue() >= 1;
    const HRESULT waitStatus = alreadyComplete
                                   ? S_OK
                                   : fence->SetEventOnCompletion(1, event);
    const DWORD wait = alreadyComplete
                           ? WAIT_OBJECT_0
                           : SUCCEEDED(waitStatus)
                                 ? WaitForSingleObject(event, 5000)
                                 : WAIT_FAILED;
    CloseHandle(event);
    if (wait != WAIT_OBJECT_0) {
      return 10;
    }
    validationMessages = SevereMessageCount(*infoQueue.Get());
    if (validationMessages != 0) {
      return 11;
    }
  }
  const auto telemetry = QueryPrimaryDxgiVideoMemoryTelemetry3D(17);
  DXGI_ADAPTER_DESC1 selectedDescription{};
  if (FAILED(adapter->GetDesc1(&selectedDescription)) ||
      telemetry.adapterLuidLow != selectedDescription.AdapterLuid.LowPart ||
      telemetry.adapterLuidHigh != selectedDescription.AdapterLuid.HighPart) {
    return 12;
  }
  if (!telemetry.adapterFound || !telemetry.hardwareAdapter ||
      !telemetry.local.querySucceeded || !telemetry.nonLocal.querySucceeded ||
      telemetry.dedicatedVideoMemoryBytes == 0 || telemetry.local.budgetBytes == 0 ||
      telemetry.queryEpoch != 17) {
    return 1;
  }
  const auto profile =
      MakeDevelopmentUltraQuality3D(DevelopmentUltraResolutionTier3D::FullHd);
  const auto admission = EvaluateDevelopmentUltraAdmission3D(
      profile, telemetry,
      {512ull * 1024ull * 1024ull, 1024ull * 1024ull * 1024ull, 0});
  std::cout << "adapterLuid=" << telemetry.adapterLuidHigh << ':'
            << telemetry.adapterLuidLow
            << " dedicatedBytes=" << telemetry.dedicatedVideoMemoryBytes
            << " localBudget=" << telemetry.local.budgetBytes
            << " localUsage=" << telemetry.local.currentUsageBytes
            << " localReservation=" << telemetry.local.currentReservationBytes
            << " nonLocalBudget=" << telemetry.nonLocal.budgetBytes
            << " nonLocalUsage=" << telemetry.nonLocal.currentUsageBytes
            << " admission=" << admission.admitted
            << " reason=" << static_cast<unsigned>(admission.reason)
            << " gpuValidation=" << gpuValidation
            << " validationWarningsOrErrors=" << validationMessages << '\n';
  return 0;
}
