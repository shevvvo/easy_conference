#include "user_interaction.h"
#include <iostream>

namespace primitives {
    Command get_user_command(std::istream &in, std::ostream &out, std::string message) {
        out << message;
        int opt;
        in >> opt;
        return static_cast<Command>(opt);
    }

    std::string get_user_input(std::istream &in, std::ostream &out, std::string message) {
        out << message;
        std::string input;
        in >> input;
        return input;
    }
}