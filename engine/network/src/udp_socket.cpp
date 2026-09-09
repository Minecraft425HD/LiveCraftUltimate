#include "lcu/network/udp_socket.h"

#include "lcu/core/log.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <cstring>

namespace lcu::network {

namespace {

#if defined(_WIN32)
// Winsock needs one process-wide WSAStartup/WSACleanup pair. Reference-
// counted here rather than requiring callers to do it themselves -
// nothing else in this codebase touches sockets, so UdpSocket is the
// only thing that needs to know Winsock exists at all.
int g_wsa_ref_count = 0;

bool ensure_wsa_initialized() {
    if (g_wsa_ref_count == 0) {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            LCU_LOG_ERROR("WSAStartup failed");
            return false;
        }
    }
    ++g_wsa_ref_count;
    return true;
}

void release_wsa() {
    --g_wsa_ref_count;
    if (g_wsa_ref_count == 0) {
        WSACleanup();
    }
}
#endif

}  // namespace

UdpSocket::UdpSocket() = default;

UdpSocket::~UdpSocket() { close(); }

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : handle_(other.handle_) { other.handle_ = kInvalidHandle; }

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        other.handle_ = kInvalidHandle;
    }
    return *this;
}

bool UdpSocket::bind(u16 port) {
    close();

#if defined(_WIN32)
    if (!ensure_wsa_initialized()) {
        return false;
    }
#endif

    const Handle sock = static_cast<Handle>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (sock < 0) {
        LCU_LOG_ERROR("UdpSocket::bind: socket() failed");
#if defined(_WIN32)
        release_wsa();
#endif
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

#if defined(_WIN32)
    if (::bind(static_cast<SOCKET>(sock), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LCU_LOG_ERROR("UdpSocket::bind: bind() failed on port {}", port);
        closesocket(static_cast<SOCKET>(sock));
        release_wsa();
        return false;
    }
    u_long non_blocking = 1;
    ioctlsocket(static_cast<SOCKET>(sock), FIONBIO, &non_blocking);
#else
    if (::bind(static_cast<int>(sock), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LCU_LOG_ERROR("UdpSocket::bind: bind() failed on port {}: {}", port, std::strerror(errno));
        ::close(static_cast<int>(sock));
        return false;
    }
    const int flags = fcntl(static_cast<int>(sock), F_GETFL, 0);
    fcntl(static_cast<int>(sock), F_SETFL, flags | O_NONBLOCK);
#endif

    handle_ = sock;
    return true;
}

bool UdpSocket::send_to(const Address& to, const std::vector<u8>& payload) {
    if (!is_open()) {
        return false;
    }
    if (payload.size() > kMaxDatagramSize) {
        LCU_LOG_DEBUG("UdpSocket::send_to: payload of {} bytes exceeds kMaxDatagramSize, dropping", payload.size());
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    std::memcpy(&addr.sin_addr.s_addr, to.ip, sizeof(addr.sin_addr.s_addr));
    addr.sin_port = htons(to.port);

#if defined(_WIN32)
    const int sent = ::sendto(static_cast<SOCKET>(handle_), reinterpret_cast<const char*>(payload.data()),
                               static_cast<int>(payload.size()), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
#else
    const auto sent = ::sendto(static_cast<int>(handle_), payload.data(), payload.size(), 0,
                                reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
#endif
    if (sent < 0 || static_cast<usize>(sent) != payload.size()) {
        LCU_LOG_DEBUG("UdpSocket::send_to: sendto() to {} failed or was truncated", to.to_string());
        return false;
    }
    return true;
}

std::optional<std::vector<u8>> UdpSocket::try_receive(Address& out_from) {
    if (!is_open()) {
        return std::nullopt;
    }

    std::vector<u8> buffer(kMaxDatagramSize);
    sockaddr_in from_addr{};
#if defined(_WIN32)
    int from_len = sizeof(from_addr);
    const int received =
        ::recvfrom(static_cast<SOCKET>(handle_), reinterpret_cast<char*>(buffer.data()),
                   static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&from_addr), &from_len);
    if (received < 0) {
        return std::nullopt;  // WSAEWOULDBLOCK (no datagram waiting) or a genuine error either way - non-fatal here
    }
#else
    socklen_t from_len = sizeof(from_addr);
    const auto received = ::recvfrom(static_cast<int>(handle_), buffer.data(), buffer.size(), 0,
                                      reinterpret_cast<sockaddr*>(&from_addr), &from_len);
    if (received < 0) {
        return std::nullopt;  // EAGAIN/EWOULDBLOCK (no datagram waiting) or a genuine error either way
    }
#endif

    buffer.resize(static_cast<usize>(received));
    std::memcpy(out_from.ip, &from_addr.sin_addr.s_addr, sizeof(out_from.ip));
    out_from.port = ntohs(from_addr.sin_port);
    return buffer;
}

void UdpSocket::close() {
    if (!is_open()) {
        return;
    }
#if defined(_WIN32)
    closesocket(static_cast<SOCKET>(handle_));
    release_wsa();
#else
    ::close(static_cast<int>(handle_));
#endif
    handle_ = kInvalidHandle;
}

}  // namespace lcu::network
