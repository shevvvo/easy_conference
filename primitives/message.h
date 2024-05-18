#ifndef HARD_CODING_MESSAGE_H
#define HARD_CODING_MESSAGE_H

#include <string>
#include <nlohmann/json.hpp>

namespace primitives {
    struct networkMessage {
        std::string command;
        std::string user;
        std::string data;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(networkMessage, command, user, data)
}

#endif //HARD_CODING_MESSAGE_H
