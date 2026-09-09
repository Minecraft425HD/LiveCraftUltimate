#pragma once

#include <optional>
#include <vector>

#include "lcu/core/types.h"
#include "lcu/network/address.h"

namespace lcu::network {

// Maximum single-datagram payload this wrapper will send/receive. Well
// under the common 1500-byte Ethernet MTU (leaving room for IP/UDP
// headers) to avoid IP fragmentation - real games generally keep UDP
// packets under ~1200 bytes for the same reason; not yet enforced by an
// assert anywhere upstream (see DECISIONS.md), just sized as the receive
// buffer here.
constexpr usize kMaxDatagramSize = 1200;

// Thin cross-platform (POSIX/Winsock, selected in the .cpp) wrapper
// around a single UDP socket. IPv4 only (see Address). Non-blocking by
// default once bound, so a server's tick loop can poll it without
// stalling on one absent packet - callers drive the polling themselves
// (no background thread), matching engine/jobs's "the caller decides
// when work happens" style rather than hiding a thread in here.
class UdpSocket : public NonCopyable {
   public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    // Binds to `port` on all local interfaces (INADDR_ANY), non-blocking.
    // Returns false (logged) on failure - port already in use, no
    // permission for a privileged port, etc.
    bool bind(u16 port);

    bool is_open() const { return handle_ != kInvalidHandle; }

    // Sends `payload` as one UDP datagram to `to`. Returns false (logged
    // at debug level, not error - a dropped/failed send on a socket that
    // is otherwise fine is an expected, routine occurrence for UDP, not
    // an exceptional condition) if the payload exceeds kMaxDatagramSize
    // or the OS call itself fails.
    bool send_to(const Address& to, const std::vector<u8>& payload);

    // Polls for one waiting datagram without blocking. Returns
    // std::nullopt if none is available right now. `out_from` receives
    // the sender's address.
    std::optional<std::vector<u8>> try_receive(Address& out_from);

    void close();

   private:
    // A plain int/SOCKET-sized handle keeps this header free of
    // platform socket headers (<winsock2.h> vs. <sys/socket.h> - see
    // udp_socket.cpp) so nothing that merely includes this file needs
    // to link against Winsock or deal with its macro pollution.
    using Handle = long long;
    static constexpr Handle kInvalidHandle = -1;

    Handle handle_ = kInvalidHandle;
};

}  // namespace lcu::network
