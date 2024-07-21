#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace primitives {
enum class Command {
    INVALID = 0,
    CREATE,
    JOIN,
    MESSAGE,
};

NLOHMANN_JSON_SERIALIZE_ENUM(
    Command,
    {
        { Command::INVALID, nullptr},
        { Command::CREATE, "create" },
        { Command::JOIN, "join" },
        { Command::MESSAGE, "message" },
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
