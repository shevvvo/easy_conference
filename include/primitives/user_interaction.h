#pragma once

#include "message.h"
#include <istream>
#include <string_view>

namespace primitives {
template <typename MessageType, typename InputStream, typename OutputStream>
MessageType get_user_input(InputStream& in, OutputStream& out, std::string_view message) {
    out << message;
    MessageType user_input;
    in >> user_input;
    return user_input;
}
} // namespace primitives
