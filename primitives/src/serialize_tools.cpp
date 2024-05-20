#include "serialize_tools.h"

namespace primitives {
    std::string serialize_json(NetworkMessage&& message) {
        return nlohmann::json(message).dump() + "\e";
    }
}