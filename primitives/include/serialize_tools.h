#ifndef HARD_CODING_SERIALIZE_TOOLS_H
#define HARD_CODING_SERIALIZE_TOOLS_H

#include "message.h"

namespace primitives {
std::string serialize_json(NetworkMessage&& message);
}

#endif // HARD_CODING_SERIALIZE_TOOLS_H
