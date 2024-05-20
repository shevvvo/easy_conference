#include <nlohmann/json.hpp>
#include "deserialize_tools.h"

namespace primitives {
    NetworkMessage deserialize_json(std::string&& message) {
        nlohmann::json req = nlohmann::json::parse(message);
        return req.template get<primitives::NetworkMessage>();
    }
}