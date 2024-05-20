#ifndef HARD_CODING_USER_INTERACTION_H
#define HARD_CODING_USER_INTERACTION_H

#include "message.h"
#include <istream>
#include <string>

namespace primitives {
Command get_user_command(std::istream& in, std::ostream& out, std::string message);

std::string get_user_input(std::istream& in, std::ostream& out, std::string message);
} // namespace primitives

#endif // HARD_CODING_USER_INTERACTION_H
