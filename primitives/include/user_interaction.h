#pragma once

#include "message.h"
#include <istream>
#include <string_view>

namespace primitives {
template <typename InputStream, typename OutputStream, typename MessageType>
void get_user_input(InputStream& in, OutputStream& out, std::string_view message, MessageType& user_input) {
    out << message;
    in >> user_input;
}
} // namespace primitives
