#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <cstdint>

namespace Network {

    enum class PacketType : std::uint16_t {
        Login = 1,
        ExecuteSql = 2,
        Response = 3,
        Error = 4
    };

    constexpr std::uint32_t kMaxPacketSize = 16 * 1024 * 1024;
    constexpr std::uint16_t kDefaultPort = 3307;

}

#endif // NETWORK_PROTOCOL_H
