#include "client.h"
#include "user_interaction.h"
#include <boost/asio.hpp>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    auto logger = spdlog::stdout_color_mt("logger");
    boost::asio::io_service service;

    std::string username{};
    primitives::get_user_input(std::cin, std::cout, "Enter your username\n", username);
    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("127.0.0.1"), 8001);
    EasyClient::create(ep, username, service, logger);

    service.run();
}
