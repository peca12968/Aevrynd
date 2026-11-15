#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Aevrynd/ReplicationProtocol3D.h>
#include <array>
#include <stdexcept>
#include <utility>

namespace Mistwatch::LocalWorldTcp {
using Aevrynd::MaximumReplicationPacketBytes3D;
constexpr std::size_t HeaderSize = 4;
constexpr std::size_t MaxBuffered = 2 * (MaximumReplicationPacketBytes3D + HeaderSize);

class Socket {
public:
    explicit Socket(SOCKET handle = INVALID_SOCKET) : handle_(handle) {}
    ~Socket() { if (*this) closesocket(handle_); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept : handle_(std::exchange(other.handle_, INVALID_SOCKET)) {}
    explicit operator bool() const { return handle_ != INVALID_SOCKET; }
    SOCKET Get() const { return handle_; }
private:
    SOCKET handle_;
};

struct Winsock {
    Winsock() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("Winsock setup failed");
    }
    ~Winsock() { WSACleanup(); }
};

inline bool Nonblocking(SOCKET socket) {
    unsigned long enabled = 1;
    return ioctlsocket(socket, FIONBIO, &enabled) == 0;
}

struct Peer {
    Socket socket;
    std::vector<std::byte> incoming, outgoing;
    std::size_t outgoingOffset{};

    bool Queue(std::span<const std::byte> bytes) {
        if (bytes.empty() || bytes.size() > MaximumReplicationPacketBytes3D) return false;
        if (outgoingOffset) {
            outgoing.erase(outgoing.begin(), outgoing.begin() + static_cast<std::ptrdiff_t>(outgoingOffset));
            outgoingOffset = 0;
        }
        if (bytes.size() + HeaderSize > MaxBuffered - outgoing.size()) return false;
        // 長度框架使用網路位元組順序，內容由既有 Aevrynd 編碼器處理。
        for (int shift : {24, 16, 8, 0}) outgoing.push_back(std::byte((bytes.size() >> shift) & 255));
        outgoing.insert(outgoing.end(), bytes.begin(), bytes.end());
        return true;
    }

    bool Flush() {
        while (outgoingOffset != outgoing.size()) {
            const int sent = send(socket.Get(), reinterpret_cast<const char*>(outgoing.data() + outgoingOffset),
                                  static_cast<int>(outgoing.size() - outgoingOffset), 0);
            if (sent == SOCKET_ERROR) return WSAGetLastError() == WSAEWOULDBLOCK;
            if (sent == 0) return false;
            outgoingOffset += static_cast<std::size_t>(sent);
        }
        outgoing.clear();
        outgoingOffset = 0;
        return true;
    }

    template<class Receive>
    bool Read(Receive&& receive) {
        std::array<std::byte, 4096> bytes;
        // 每個連線每輪最多讀取一段資料，讓其他玩家與世界更新有機會執行。
        const int n = recv(socket.Get(), reinterpret_cast<char*>(bytes.data()), static_cast<int>(bytes.size()), 0);
        if (n == SOCKET_ERROR) return WSAGetLastError() == WSAEWOULDBLOCK;
        if (n == 0 || static_cast<std::size_t>(n) > MaxBuffered - incoming.size()) return false;
        incoming.insert(incoming.end(), bytes.begin(), bytes.begin() + n);
        std::size_t used = 0;
        while (incoming.size() - used >= HeaderSize) {
            std::size_t length = 0;
            for (auto byte : std::span(incoming).subspan(used, HeaderSize))
                length = (length << 8) | std::to_integer<unsigned>(byte);
            if (length == 0 || length > MaximumReplicationPacketBytes3D) return false;
            if (incoming.size() - used - HeaderSize < length) break;
            if (!receive(std::span(incoming).subspan(used + HeaderSize, length))) return false;
            used += HeaderSize + length;
        }
        incoming.erase(incoming.begin(), incoming.begin() + static_cast<std::ptrdiff_t>(used));
        return true;
    }
};

} // namespace Mistwatch::LocalWorldTcp
