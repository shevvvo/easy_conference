#include "chat/server.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::unordered_map<std::string, std::vector<std::shared_ptr<EasyServer>>> rooms;
    boost::asio::io_service service;
    boost::uuids::random_generator random_generator_;
    auto logger = spdlog::stdout_color_mt("logger");
    boost::asio::ip::tcp::acceptor acceptor(service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8001));

    logger->info("Server started");
    std::shared_ptr<EasyServer> client = EasyServer::create(service, rooms, random_generator_, logger);
    acceptor.async_accept(
        client->getSocket(),
        [client = client,
         &acceptor = acceptor,
         &service = service,
         &rooms = rooms,
         &generator = random_generator_,
         &logger = logger](const boost::system::error_code& err) {
            EasyServer::handle_accept(client, err, acceptor, service, rooms, generator, logger);
        }
    );
    service.run();
}
