#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp> 
#include <iostream>
#include <boost/noncopyable.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "message.h"
#include "user_interaction.cpp"

class serverConnection;
using ClientPtr = std::shared_ptr<serverConnection>;
using RoomsMap = std::unordered_map<std::string, std::vector<ClientPtr>>;
using Acceptor = boost::asio::ip::tcp::acceptor;
using ErrorCode = boost::system::error_code;

class serverConnection : public std::enable_shared_from_this<serverConnection>, boost::noncopyable {
    serverConnection(boost::asio::io_service& service, RoomsMap& rooms, boost::uuids::random_generator& generator) : sock_(service), started_(false), rooms_(rooms), random_generator_(generator) {}
public:
    typedef std::shared_ptr<serverConnection> ptr;

    void start() {
        started_ = true;
        do_read();
    }
    static ptr create(boost::asio::io_service& service, RoomsMap& rooms, boost::uuids::random_generator& generator) {
        ptr new_client(new serverConnection(service, rooms, generator));
        return new_client;
    }
    void stop() {
        spdlog::get("logger")->info("Stopping server connection");
        if ( !started_) return;
        started_ = false;
        sock_.close();

        ptr self = shared_from_this();
        auto it = rooms_.find(room_id_);
        if (it != rooms_.end()) {
            auto it2 = std::find(it->second.begin(), it->second.end(), self);
            it->second.erase(it2);
        }
    }
    bool started() const { return started_; }
    boost::asio::ip::tcp::socket & sock() { return sock_;}
    std::string getUsername() const { return username_; }
private:
    void on_read(const ErrorCode& err, size_t bytes) {
        if ( err) stop();
        if ( !started() ) return;
        if (!bytes) return;
        nlohmann::json req = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        auto req_struct = req.template get<primitives::NetworkMessage>();
        spdlog::get("logger")->info("Read the message: " + req.dump());

        switch (req_struct.command) {
            case primitives::Command::CMD_JOIN: {
                std::string id = req_struct.data;
                auto it = rooms_.find(id);
                if (it != rooms_.end()) {
                    room_id_ = id;
                    it->second.push_back(shared_from_this());
                    primitives::NetworkMessage answer = {primitives::Command::CMD_JOIN, "", "success"};
                    auto dump = nlohmann::json(answer).dump() + "\e";
                    username_ = req_struct.user;
                    sock_.async_write_some(boost::asio::buffer(dump, dump.size()),
                                           [shared_this = shared_from_this()](const ErrorCode&err, size_t bytes) {shared_this->on_write(err, bytes);});

                } else {
                    primitives::NetworkMessage answer = {primitives::Command::CMD_JOIN, "", "fail"};
                    auto dump = nlohmann::json(answer).dump() + "\e";
                    sock_.async_write_some(boost::asio::buffer(dump, dump.size()),
                                           [shared_this = shared_from_this()](const ErrorCode&err, size_t bytes) {shared_this->on_write(err, bytes);});
                }
                break;
            }
            case primitives::Command::CMD_CREATE: {
                room_id_ = boost::uuids::to_string(random_generator_());
                std::vector<ClientPtr> vec{ shared_from_this() };
                rooms_.insert_or_assign(room_id_, vec);
                primitives::NetworkMessage answer = {primitives::Command::CMD_CREATE, "", room_id_};
                auto dump = nlohmann::json(answer).dump() + "\e";
                username_ = req_struct.user;
                sock_.async_write_some(boost::asio::buffer(dump, dump.size()),
                                       [shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {shared_this->on_write(err, bytes);});
                break;
            }
            case primitives::Command::CMD_MESSAGE: {
                if (!room_id_.empty()) {
                    auto it = rooms_.find(room_id_);
                    if (it != rooms_.end()) {
                        for (auto& elem : it->second) {
                            std::string nmm = elem->getUsername();
                            if (elem->getUsername() != username_) {
                                std::string dump = req.dump() + "\e";
                                elem->sock().async_write_some(boost::asio::buffer(dump, dump.size()),
                                                              [shared_this = shared_from_this()](const ErrorCode&err,size_t bytes) {shared_this->on_write(err, bytes); });
                            }
                        }
                    }
                }
                break;
            }
            default: {
                break;
            }
        }
        do_read();
    }

    void on_write(const ErrorCode& err, size_t bytes) {
        if (err) {
            stop();
        }
    }
    void do_read() {
        async_read(sock_, boost::asio::buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code& err, std::size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const boost::system::error_code& err, std::size_t bytes) {shared_this->on_read(err, bytes);});
    }
    void do_write(const std::string & msg) {
        if (!started()) {
            return;
        }
        std::copy(msg.begin(), msg.end(), write_buffer_);
        sock_.async_write_some( boost::asio::buffer(write_buffer_, msg.size()),[shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {shared_this->on_write(err, bytes);});
    }
    size_t read_complete(const boost::system::error_code & err, size_t bytes) {
        if (err) {
            return 0;
        }
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\e') < read_buffer_ + bytes;
        return found ? 0 : 1;
    }
private:
    std::string username_;
    std::string room_id_;
    boost::asio::ip::tcp::socket sock_;
    static constexpr int max_msg = 1024;
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
    RoomsMap& rooms_;
    boost::uuids::random_generator& random_generator_;
};

void handle_accept(serverConnection::ptr client, const boost::system::error_code & err, boost::asio::ip::tcp::acceptor& acceptor, boost::asio::io_service& service, RoomsMap& rooms, boost::uuids::random_generator& generator) {
    spdlog::get("logger")->info("Handling accept");
    client->start();
    serverConnection::ptr new_client = serverConnection::create(service, rooms, generator);
    acceptor.async_accept(new_client->sock(), [client = new_client, &acceptor = acceptor, &service = service, &rooms = rooms, &generator = generator]
    (const boost::system::error_code & err){ handle_accept(client, err, acceptor, service, rooms, generator);});
}

int main(int argc, char* argv[]) {
    RoomsMap rooms;
    boost::asio::io_service service;
    boost::uuids::random_generator random_generator_;
    auto logger = spdlog::stdout_color_mt("logger");
    boost::asio::ip::tcp::acceptor acceptor(service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8001));

    logger->info("Server started");
    serverConnection::ptr client = serverConnection::create(service, rooms, random_generator_);
    acceptor.async_accept(client->sock(), [client = client, &acceptor = acceptor, &service = service, &rooms = rooms, &generator = random_generator_]
    (const boost::system::error_code & err){ handle_accept(client, err, acceptor, service, rooms, generator);});
    service.run();
}
