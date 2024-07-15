#ifndef HARD_CODING_USER_INTERACTION_H
#define HARD_CODING_USER_INTERACTION_H

#include "message.h"
#include <istream>
#include <string>

namespace primitives {
template <typename InputStream, typename OutputStream, typename MessageType>
void get_user_input(InputStream& in, OutputStream& out, std::string message, MessageType& user_input) {
    out << message;
    in >> user_input;
}
} // namespace primitives

#endif // HARD_CODING_USER_INTERACTION_H
