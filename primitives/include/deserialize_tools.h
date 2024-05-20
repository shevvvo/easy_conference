#ifndef HARD_CODING_DESERIALIZE_TOOLS_H
#define HARD_CODING_DESERIALIZE_TOOLS_H

#include "message.h"

namespace primitives {
NetworkMessage deserialize_json(std::string&& message);
}

#endif // HARD_CODING_DESERIALIZE_TOOLS_H
