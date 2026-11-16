#pragma once

#include "RiverbankProfileStore.h"
#include <algorithm>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

namespace Mistwatch {

class RiverbankProfileRegistration {
public:
    struct Result {
        std::optional<RiverbankProfile> profile;
        RiverbankTicket ticket{};
        bool databaseError{};
    };

    RiverbankProfileRegistration(RiverbankProfileStore& store, std::size_t capacity)
        : connection_(store.OpenRegistrationConnection()), capacity_(capacity),
          worker_([this](std::stop_token stop) { Register(stop); }) {}

    bool Submit(std::uint32_t owner, const RiverbankTicket& ticket, RiverbankClass role) {
        std::scoped_lock lock(mutex_);
        if (entries_.size() >= capacity_ ||
            !entries_.try_emplace(owner, Entry{Request{ticket, role}, {}}).second) return false;
        changed_.notify_one();
        return true;
    }

    void Cancel(std::uint32_t owner) {
        std::scoped_lock lock(mutex_);
        entries_.erase(owner);
    }

    std::optional<Result> Take(std::uint32_t owner) {
        std::scoped_lock lock(mutex_);
        const auto entry = entries_.find(owner);
        if (entry == entries_.end() || !entry->second.result) return std::nullopt;
        auto result = std::move(entry->second.result);
        entries_.erase(entry);
        return result;
    }

private:
    struct Request {
        RiverbankTicket ticket;
        RiverbankClass role;
    };
    struct Entry {
        std::optional<Request> pending;
        std::optional<Result> result;
    };

    void Register(std::stop_token stop) {
        for (;;) {
            std::uint32_t owner{};
            Request request{};
            {
                std::unique_lock lock(mutex_);
                const auto queued = [this] {
                    return std::ranges::find_if(entries_, [](const auto& entry) { return entry.second.pending.has_value(); });
                };
                if (!changed_.wait(lock, stop, [&] { return queued() != entries_.end(); }) || stop.stop_requested()) return;
                const auto entry = queued();
                owner = entry->first;
                request = *entry->second.pending;
                entry->second.pending.reset();
            }

            Result result;
            try {
                result.profile = connection_->Create(request.ticket, request.role);
                if (result.profile) result.ticket = request.ticket;
            } catch (const std::exception&) {
                result.databaseError = true;
            }

            std::scoped_lock lock(mutex_);
            const auto entry = entries_.find(owner);
            // 連線中止會移除工作。已完成的交易仍可用原角色票證重新登入。
            if (entry != entries_.end()) entry->second.result = std::move(result);
        }
    }

    std::unique_ptr<RiverbankProfileStore> connection_;
    std::size_t capacity_;
    std::mutex mutex_;
    std::condition_variable_any changed_;
    std::map<std::uint32_t, Entry> entries_;
    std::jthread worker_;
};

}
