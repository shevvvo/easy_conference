#include "primitives/common_json.h"
#include <nlohmann/json.hpp>

namespace primitives {
NetworkMessage deserialize_json(std::string&& message) {
    nlohmann::json req = nlohmann::json::parse(message);
    return req.template get<primitives::NetworkMessage>();
}

std::string serialize_json(NetworkMessage&& message) { return nlohmann::json(message).dump() + "\n"; }
} // namespace primitives