#pragma once

#include "message.h"

namespace primitives {
NetworkMessage deserialize_json(std::string&& message);

std::string serialize_json(NetworkMessage&& message);
} // namespace primitives
