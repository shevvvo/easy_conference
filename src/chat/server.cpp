#include "chat/server.h"

#include <utility>

EasyServer::EasyServer(
    boost::asio::io_service& service,
    std::unordered_map<std::string, std::vector<std::shared_ptr<EasyServer>>>& rooms,
    boost::uuids::random_generator& generator,
    std::shared_ptr<spdlog::logger> logger
)
    : sock_(service), started_(false), rooms_(rooms), random_generator_(generator), logger_(std::move(logger)) {}

void EasyServer::start() {
    started_ = true;
    do_read();
}

std::shared_ptr<EasyServer> EasyServer::create(
    boost::asio::io_service& service,
    std::unordered_map<std::string, std::vector<std::shared_ptr<EasyServer>>>& rooms,
    boost::uuids::random_generator& generator,
    std::shared_ptr<spdlog::logger> logger
) {
    std::shared_ptr<EasyServer> new_client(new EasyServer(service, rooms, generator, std::move(logger)));
    return new_client;
}

void EasyServer::stop() {
    spdlog::get("logger")->info("Stopping server connection");
    if (!started_) return;
    started_ = false;
    sock_.close();

    std::shared_ptr<EasyServer> self = shared_from_this();
    auto it = rooms_.find(room_id_);
    if (it != rooms_.end()) {
        auto it2 = std::find(it->second.begin(), it->second.end(), self);
        it->second.erase(it2);
    }
}

void EasyServer::on_read(const boost::system::error_code& err, size_t bytes) {
    if (err) {
        stop();
        return;
    }
    if (!getStarted()) return;
    if (!bytes) return;
    std::string req_str = std::string(read_buffer_, bytes - 1);
    auto req_struct = primitives::deserialize_json(std::forward<std::string>(req_str));
    spdlog::get("logger")->info("Read the message: " + req_str);

    switch (req_struct.command) {
    case primitives::Command::JOIN: {
        std::string id = req_struct.data;
        auto it = rooms_.find(id);
        if (it != rooms_.end()) {
            room_id_ = id;
            it->second.push_back(shared_from_this());
            auto answer = primitives::serialize_json(primitives::NetworkMessage{
                .command = primitives::Command::JOIN, .user = "", .data = "success" });
            username_ = req_struct.user;
            sock_.async_write_some(
                boost::asio::buffer(answer, answer.size()),
                [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
                    shared_this->on_write(err_, bytes_);
                }
            );

        } else {
            auto answer = primitives::serialize_json(primitives::NetworkMessage{
                .command = primitives::Command::JOIN, .user = "", .data = "fail" });
            sock_.async_write_some(
                boost::asio::buffer(answer, answer.size()),
                [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
                    shared_this->on_write(err_, bytes_);
                }
            );
        }
        break;
    }
    case primitives::Command::CREATE: {
        room_id_ = boost::uuids::to_string(random_generator_());
        std::vector<std::shared_ptr<EasyServer>> vec{ shared_from_this() };
        rooms_.insert_or_assign(room_id_, std::move(vec));
        auto answer = primitives::serialize_json(primitives::NetworkMessage{
            .command = primitives::Command::CREATE, .user = "", .data = room_id_ });
        username_ = req_struct.user;
        sock_.async_write_some(
            boost::asio::buffer(answer, answer.size()),
            [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
                shared_this->on_write(err_, bytes_);
            }
        );
        break;
    }
    case primitives::Command::MESSAGE: {
        if (room_id_.empty()) {
            break;
        }
        auto it = rooms_.find(room_id_);

        if (it == rooms_.end()) {
            break;
        }
        for (auto& elem : it->second) {
            std::string nmm = elem->getUsername();
            if (elem->getUsername() == username_) {
                continue;
            }
            std::string dump = req_str + "\n";
            elem->getSocket().async_write_some(
                boost::asio::buffer(dump, dump.size()),
                [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
                    shared_this->on_write(err_, bytes_);
                }
            );
        }
        break;
    }
    default: {
        break;
    }
    }
    do_read();
}

void EasyServer::on_write(const boost::system::error_code& err, [[maybe_unused]] size_t bytes) {
    if (err) {
        stop();
        return;
    }
}

void EasyServer::do_read() {
    async_read(
        sock_,
        boost::asio::buffer(read_buffer_),
        [shared_this = shared_from_this()](const boost::system::error_code& err, std::size_t bytes) {
            return shared_this->read_complete(err, bytes);
        },
        [shared_this = shared_from_this()](const boost::system::error_code& err, std::size_t bytes) {
            shared_this->on_read(err, bytes);
        }
    );
}

void EasyServer::do_write(const std::string& msg) {
    if (!getStarted()) {
        return;
    }
    std::copy(msg.begin(), msg.end(), write_buffer_);
    sock_.async_write_some(
        boost::asio::buffer(write_buffer_, msg.size()),
        [shared_this = shared_from_this()](const boost::system::error_code& err, size_t bytes) {
            shared_this->on_write(err, bytes);
        }
    );
}

size_t EasyServer::read_complete(const boost::system::error_code& err, size_t bytes) {
    if (err) {
        return 0;
    }
    bool found = std::find(read_buffer_, read_buffer_ + bytes, '\n') < read_buffer_ + bytes;
    return found ? 0 : 1;
}

void EasyServer::handle_accept(
    const std::shared_ptr<EasyServer>& client,
    [[maybe_unused]] const boost::system::error_code& err,
    boost::asio::ip::tcp::acceptor& acceptor,
    boost::asio::io_service& service,
    std::unordered_map<std::string, std::vector<std::shared_ptr<EasyServer>>>& rooms,
    boost::uuids::random_generator& generator,
    std::shared_ptr<spdlog::logger>& logger
) {
    logger->info("Handling accept");
    client->start();
    std::shared_ptr<EasyServer> new_client = EasyServer::create(service, rooms, generator, logger);
    acceptor.async_accept(
        new_client->getSocket(),
        [client = new_client,
         &acceptor = acceptor,
         &service = service,
         &rooms = rooms,
         &generator = generator,
         &logger = logger](const boost::system::error_code& err_) {
            handle_accept(client, err_, acceptor, service, rooms, generator, logger);
        }
    );
}