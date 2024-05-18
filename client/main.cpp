#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>
#include <boost/noncopyable.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include "message.h"

auto console = spdlog::stdout_color_mt("console");
boost::asio::io_service service;

class clientConnection : public boost::enable_shared_from_this<clientConnection>, boost::noncopyable {
    explicit clientConnection(std::string& username)
            : sock_(service), started_(true), input_stream_(service, STDIN_FILENO), username_(username) {}

    void start(const boost::asio::ip::tcp::endpoint& ep) {
        sock_.async_connect(ep, [shared_this = shared_from_this()](const error_code & err) {
            shared_this->on_connect(err);
        });
    }

public:
    typedef boost::system::error_code error_code;
    typedef std::shared_ptr<clientConnection> ptr;

    static ptr create(const boost::asio::ip::tcp::endpoint& ep, std::string& username) {
        ptr new_(new clientConnection(username));
        new_->start(ep);
        return new_;
    }
    void stop() {
        if ( !started_) return;
        started_ = false;
        sock_.close();
    }
    bool started() const { return started_; }
private:
    void on_connect(const error_code & err) {
        if ( err)      stop();
        console->info("Connected");
        std::cout << "Choose option:\n";
        std::cout << "1. Create new room\n";
        std::cout << "2. Join existing room\n";
        int opt;
        std::cin >> opt;
        switch (opt) {
            case 1: {
                primitives::networkMessage newMsg = {"create", username_, ""};
                auto dump = nlohmann::json(newMsg).dump();
                sock_.async_write_some( boost::asio::buffer(dump + "\e", dump.size() + 1),[shared_this = shared_from_this()](const error_code & err, size_t bytes) {
                    shared_this->on_create_sent(err, bytes);
                });
                break;
            }
            case 2: {
                std::cout << "Enter chat id: ";
                std::string chat_id;
                std::cin >> chat_id;
                primitives::networkMessage newMsg = {"join", username_, chat_id};
                auto dump = nlohmann::json(newMsg).dump();
                sock_.async_write_some( boost::asio::buffer(dump + "\e", dump.size() + 1),[shared_this = shared_from_this()](const error_code & err, size_t bytes) {
                    shared_this->on_join_sent(err, bytes);
                });
                break;
            }
            default: {
                break;
            }
        }
    }

    void on_create_sent(const error_code & err, size_t bytes) {
        if (err) stop();
        async_read(sock_, boost::asio::buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_create_read(err, bytes);});
    }

    void on_create_read(const error_code & err, size_t bytes) {
        if (err) stop();
        nlohmann::json answer = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        auto newMessage = answer.template get<primitives::networkMessage>();
        if (newMessage.command == "create") {
            if (!newMessage.data.empty()) {
                console->info("New room created: " + newMessage.data);
                read_from_input();
                read_from_socket();
            } else {
                console->info("Failed to create new room");
                stop();
            }
        }
    }

    void on_join_sent(const error_code & err, size_t bytes) {
        if (err) stop();
        async_read(sock_, boost::asio::buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_join_read(err, bytes);});
    }

    void on_join_read(const error_code & err, size_t bytes) {
        std::cout << "Join read " + std::string(read_buffer_, bytes - 1) + "\n";
        console->info("Join read " + std::string(read_buffer_, bytes - 1));
        if (err) stop();
        nlohmann::json answer = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        auto newMessage = answer.template get<primitives::networkMessage>();
        if (newMessage.command == "join") {
            if (newMessage.data == "success") {
                read_from_input();
                read_from_socket();
            } else {
                console->info("Failed to join");
                stop();
            }
        }
    }

    void on_read(const error_code & err, size_t bytes) {
        if (err) stop();
        if (!started()) return;
        nlohmann::json answer = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        console->info(answer.dump());
        read_from_socket();
    }

    void on_write(const error_code & err, size_t bytes) {
        if (err) stop();
        if (!started()) return;
        read_from_input();
    }

    void do_write(const error_code & err, size_t bytes) {
        if (err) stop();
        if ( !started() ) return;
        primitives::networkMessage newMsg = {"message", username_, std::string(input_buffer_, bytes)};
        auto dump = nlohmann::json(newMsg).dump();
        sock_.async_write_some( boost::asio::buffer(dump + "\e", dump.size() + 1),
                                [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_write(err, bytes);});
    }

    size_t read_complete(const boost::system::error_code & err, size_t bytes) {
        if ( err) return 0;
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\e') < read_buffer_ + bytes;
        return found ? 0 : 1;
    }

    size_t input_complete(const boost::system::error_code & err, size_t bytes) {
        if ( err) return 0;
        bool found = std::find(input_buffer_, input_buffer_ + bytes, '\n') < input_buffer_ + bytes;
        return found ? 0 : 1;
    }

    void read_from_input() {
        async_read(input_stream_, boost::asio::buffer(input_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->input_complete(err, bytes);},
                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->do_write(err, bytes);});
    }

    void read_from_socket() {
        async_read(sock_, boost::asio::buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_read(err, bytes);});
    }

private:
    boost::asio::ip::tcp::socket sock_;
    boost::asio::posix::stream_descriptor input_stream_;
    enum { max_msg = 1024 };
    char read_buffer_[max_msg];
    char input_buffer_[max_msg];
    bool started_;
    std::string username_;
};

int main(int argc, char* argv[]) {
    std::string username;
    std::cout << "Enter your username\n";
    std::cin >> username;
    boost::asio::ip::tcp::endpoint ep( boost::asio::ip::address::from_string("127.0.0.1"), 8001);
    clientConnection::create(ep, username);

    service.run();
}
