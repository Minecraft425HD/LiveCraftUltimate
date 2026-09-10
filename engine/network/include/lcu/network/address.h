#pragma once

#include <string>

#include "lcu/core/types.h"

namespace lcu::network {

// IPv4 endpoint only (brief section 63 doesn't call out IPv6, and
// nothing in this codebase needs it yet - see DECISIONS.md). `ip` is
// stored host-byte-order-agnostic as 4 octets so callers never have to
// think about endianness for the address itself; `port` is host byte
// order (converted to/from network byte order only at the socket
// boundary, same as `port`'s treatment in the wire header).
struct Address {
    u8 ip[4] = {0, 0, 0, 0};
    u16 port = 0;

    constexpr bool operator==(const Address& rhs) const {
        return ip[0] == rhs.ip[0] && ip[1] == rhs.ip[1] && ip[2] == rhs.ip[2] && ip[3] == rhs.ip[3] &&
               port == rhs.port;
    }
    constexpr bool operator!=(const Address& rhs) const { return !(*this == rhs); }

    static constexpr Address loopback(u16 port) { return Address{{127, 0, 0, 1}, port}; }

    std::string to_string() const;
};

}  // namespace lcu::network

namespace std {

template <>
struct hash<lcu::network::Address> {
    std::size_t operator()(const lcu::network::Address& addr) const noexcept {
        std::size_t h = static_cast<std::size_t>(addr.port);
        for (lcu::u8 octet : addr.ip) {
            h = h * 486187739u + static_cast<std::size_t>(octet);
        }
        return h;
    }
};

}  // namespace std
