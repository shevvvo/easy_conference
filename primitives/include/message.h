#ifndef HARD_CODING_MESSAGE_H
#define HARD_CODING_MESSAGE_H

#include <string>
#include <nlohmann/json.hpp>

namespace primitives {
    enum Command {
        CMD_CREATE=1,
        CMD_JOIN,
        CMD_MESSAGE,
    };

    NLOHMANN_JSON_SERIALIZE_ENUM( Command, {
        {CMD_CREATE, "create"},
        {CMD_JOIN, "join"},
        {CMD_MESSAGE, "message"},
    })

    struct NetworkMessage {
        Command command;
        std::string user;
        std::string data;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NetworkMessage, command, user, data)
}

#endif //HARD_CODING_MESSAGE_H
