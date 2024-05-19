#include "user_interaction.h"
#include <iostream>

namespace primitives {
    Command get_user_command(std::istream& in) {
        std::cout << "Choose option:\n";
        std::cout << "1. Create new room\n";
        std::cout << "2. Join existing room\n";
        int opt;
        in >> opt;
        return static_cast<Command>(opt);
    }

    std::string get_user_input(std::istream& in, std::ostream& out, std::string message) {
        out << message;
        std::string input;
        in >> input;
        return input;
    }
}