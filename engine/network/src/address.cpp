#include "lcu/network/address.h"

#include "lcu/core/log.h"

namespace lcu::network {

std::string Address::to_string() const {
    // uint8_t octets are cast to unsigned explicitly - fmt formats
    // unsigned char/uint8_t as a character by default (the traditional
    // iostream-inherited behavior), not as a number, which would print
    // unprintable bytes instead of "192.168...".
    return fmt::format("{}.{}.{}.{}:{}", static_cast<unsigned>(ip[0]), static_cast<unsigned>(ip[1]),
                        static_cast<unsigned>(ip[2]), static_cast<unsigned>(ip[3]), port);
}

}  // namespace lcu::network
