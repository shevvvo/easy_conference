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

boost::asio::io_service service;
auto console = spdlog::stdout_color_mt("console");

class serverConnection;
typedef std::shared_ptr<serverConnection> client_ptr;
std::unordered_map<std::string, std::vector<client_ptr>> rooms_;

class serverConnection : public std::enable_shared_from_this<serverConnection>, boost::noncopyable {
    serverConnection() : sock_(service), started_(false) {
    }
public:
    typedef boost::system::error_code error_code;
    typedef std::shared_ptr<serverConnection> ptr;

    void start() {
        started_ = true;
        do_read();
    }
    static ptr new_() {
        ptr new_client(new serverConnection);
        return new_client;
    }
    void stop() {
        console->info("Stopping server connection");
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
    void on_read(const error_code & err, size_t bytes) {
        if ( err) stop();
        if ( !started() ) return;
        if (!bytes) return;
        nlohmann::json req = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        auto reqStruct = req.template get<primitives::networkMessage>();
        console->info("Read the message: " + req.dump());

        if (reqStruct.command == "join") {
            std::string id = reqStruct.data;
            auto it = rooms_.find(id);
            if (it != rooms_.end()) {
                room_id_ = id;
                it->second.push_back(shared_from_this());
                primitives::networkMessage answer = {"join", "", "success"};
                auto dump = nlohmann::json(answer).dump();
                username_ = reqStruct.user;
                sock_.async_write_some(boost::asio::buffer(dump + "\e", dump.size() + 1),
                                       [shared_this = shared_from_this()](const error_code &err, size_t bytes) {shared_this->on_write(err, bytes);});

            } else {
                primitives::networkMessage answer = {"join", "", "fail"};
                auto dump = nlohmann::json(answer).dump();
                sock_.async_write_some(boost::asio::buffer(dump + "\e", dump.size() + 1),
                                       [shared_this = shared_from_this()](const error_code &err, size_t bytes) {shared_this->on_write(err, bytes);});
            }
        } else if (reqStruct.command == "create") {
            boost::uuids::uuid uuid = boost::uuids::random_generator()();
            room_id_ = boost::uuids::to_string(uuid);
            std::vector<client_ptr> vec = {shared_from_this()};
            rooms_[room_id_] = vec;
            primitives::networkMessage answer = {"create", "", "room_id_"};
            auto dump = nlohmann::json(answer).dump();
            username_ = reqStruct.user;
            sock_.async_write_some(boost::asio::buffer(dump + "\e", dump.size() + 1),
                                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_write(err, bytes);});
        } else {
            if (!room_id_.empty()) {
                auto it = rooms_.find(room_id_);
                if (it != rooms_.end()) {
                    std::string strr = std::string(read_buffer_, bytes);
                    for (auto& elem : it->second) {
                        std::string nmm = elem->getUsername();
                        if (elem->getUsername() != username_) {
                            std::string dump = req.dump();
                            elem->sock().async_write_some(boost::asio::buffer(dump + "\e", dump.size() + 1),
                                                          [shared_this = shared_from_this()](const error_code &err,size_t bytes) {shared_this->on_write(err, bytes); });
                        }
                    }
                }
            }
        }
        do_read();
    }

    void on_write(const error_code & err, size_t bytes) {
        if (err) stop();
    }
    void do_read() {
        async_read(sock_, boost::asio::buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code& err, std::size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const boost::system::error_code& err, std::size_t bytes) {shared_this->on_read(err, bytes);});
    }
    void do_write(const std::string & msg) {
        if ( !started() ) return;
        std::copy(msg.begin(), msg.end(), write_buffer_);
        sock_.async_write_some( boost::asio::buffer(write_buffer_, msg.size()),[shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_write(err, bytes);});
    }
    size_t read_complete(const boost::system::error_code & err, size_t bytes) {
        if ( err) {
            return 0;
        }
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\e') < read_buffer_ + bytes;
        return found ? 0 : 1;
    }
private:
    std::string username_;
    std::string room_id_;
    boost::asio::ip::tcp::socket sock_;
    enum { max_msg = 1024 };
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
};

boost::asio::ip::tcp::acceptor acceptor(service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8001));

void handle_accept(serverConnection::ptr client, const boost::system::error_code & err) {
    console->info("Handling accept");
    client->start();
    serverConnection::ptr new_client = serverConnection::new_();
    acceptor.async_accept(new_client->sock(), [client = new_client](const boost::system::error_code & err){ handle_accept(client, err);});
}

int main(int argc, char* argv[]) {
    console->info("Server started");
    serverConnection::ptr client = serverConnection::new_();
    acceptor.async_accept(client->sock(), [client = client](const boost::system::error_code & err){ handle_accept(client, err);});
    service.run();
}
