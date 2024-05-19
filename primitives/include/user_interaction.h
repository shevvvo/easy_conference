#ifndef HARD_CODING_USER_INTERACTION_H
#define HARD_CODING_USER_INTERACTION_H

#include <istream>
#include <string>
#include "message.h"

namespace primitives {
    Command get_user_command(std::istream& in);
    std::string get_user_input(std::istream& in, std::ostream& out, std::string message);
}

#endif //HARD_CODING_USER_INTERACTION_H
