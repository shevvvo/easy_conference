#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace primitives {
enum Command {
    CMD_CREATE = 1,
    CMD_JOIN,
    CMD_MESSAGE,
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    Command,
    {
        { CMD_CREATE, "create" },
        { CMD_JOIN, "join" },
        { CMD_MESSAGE, "message" },
    }
)

struct NetworkMessage {
    Command command;
    std::string user;
    std::string data;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NetworkMessage, command, user, data)
} // namespace primitives

inline std::istream& operator>>(std::istream& input, primitives::Command& cmd) {
    int result;
    input >> result;
    cmd = static_cast<primitives::Command>(result);
    return input;
}
