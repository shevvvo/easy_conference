#pragma once

#include "message.h"

namespace primitives {
NetworkMessage deserialize_json(std::string&& message);

std::string serialize_json(NetworkMessage&& message);

//TODO: pack/unpack message with '\n'. State-machine
} // namespace primitives
