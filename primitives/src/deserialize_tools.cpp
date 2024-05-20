#include "deserialize_tools.h"
#include <nlohmann/json.hpp>

namespace primitives {
NetworkMessage deserialize_json(std::string&& message) {
    nlohmann::json req = nlohmann::json::parse(message);
    return req.template get<primitives::NetworkMessage>();
}
} // namespace primitives