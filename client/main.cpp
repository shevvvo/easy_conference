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
#include "user_interaction.cpp"

class clientConnection : public std::enable_shared_from_this<clientConnection>, boost::noncopyable {
    explicit clientConnection(std::string& username, boost::asio::io_service& service)
            : sock_(service), started_(true), input_stream_(service, STDIN_FILENO), username_(username) {}

    void start(const boost::asio::ip::tcp::endpoint& ep) {
        sock_.async_connect(ep, [shared_this = shared_from_this()](const ErrorCode& err) {
            shared_this->on_connect(err);
        });
    }

public:
    using ErrorCode = boost::system::error_code;
    using ClientPtr = std::shared_ptr<clientConnection>;

    static ClientPtr create(const boost::asio::ip::tcp::endpoint& ep, std::string& username, boost::asio::io_service& service) {
        ClientPtr new_(new clientConnection(username, service));
        new_->start(ep);
        return new_;
    }
    void stop() {
        if (!started_) {
            return;
        }
        started_ = false;
        sock_.close();
    }
    bool started() const { return started_; }
private:
    void on_connect(const ErrorCode& err) {
        if (err) {
            stop();
        }
        spdlog::get("logger")->info("Connected");
        primitives::Command opt = primitives::get_user_command(std::cin);
        switch (opt) {
            case primitives::Command::CMD_CREATE: {
                primitives::NetworkMessage new_msg = {primitives::Command::CMD_CREATE, username_, ""};
                auto dump = nlohmann::json(new_msg).dump() + "\e";
                sock_.async_write_some( boost::asio::buffer(dump, dump.size()),[shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {
                    shared_this->on_create_sent(err, bytes);
                });
                break;
            }
            case primitives::Command::CMD_JOIN: {
                std::string chat_id = primitives::get_user_input(std::cin, std::cout, "Enter chat id: ");
                primitives::NetworkMessage new_msg = {primitives::Command::CMD_JOIN, username_, chat_id};
                auto dump = nlohmann::json(new_msg).dump() + "\e";
                sock_.async_write_some( boost::asio::buffer(dump, dump.size()),[shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {
                    shared_this->on_join_sent(err, bytes);
                });
                break;
            }
            default: {
                break;
            }
        }
    }

    void on_create_sent(const ErrorCode& err, size_t bytes) {
        if (err) {
            stop();
        }
        async_read(sock_, boost::asio::buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {shared_this->on_create_read(err, bytes);});
    }

    void on_create_read(const ErrorCode& err, size_t bytes) {
        if (err) {
            stop();
        }
        nlohmann::json answer = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        auto new_msg = answer.template get<primitives::NetworkMessage>();
        if (new_msg.command == primitives::Command::CMD_CREATE) {
            if (!new_msg.data.empty()) {
                spdlog::get("logger")->info("New room created: " + new_msg.data);
                read_from_input();
                read_from_socket();
            } else {
                spdlog::get("logger")->info("Failed to create new room");
                stop();
            }
        }
    }

    void on_join_sent(const ErrorCode& err, size_t bytes) {
        if (err) {
            stop();
        }
        async_read(sock_, boost::asio::buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {shared_this->on_join_read(err, bytes);});
    }

    void on_join_read(const ErrorCode& err, size_t bytes) {
        spdlog::get("logger")->info("Join read " + std::string(read_buffer_, bytes - 1));
        if (err) {
            stop();
        }
        nlohmann::json answer = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        auto new_msg = answer.template get<primitives::NetworkMessage>();
        if (new_msg.command == primitives::Command::CMD_JOIN) {
            if (new_msg.data == "success") {
                read_from_input();
                read_from_socket();
            } else {
                spdlog::get("logger")->info("Failed to join");
                stop();
            }
        }
    }

    void on_read(const ErrorCode& err, size_t bytes) {
        if (err) {
            stop();
        }
        if (!started()) {
            return;
        }
        nlohmann::json answer = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        spdlog::get("logger")->info(answer.dump());
        read_from_socket();
    }

    void on_write(const ErrorCode& err, size_t bytes) {
        if (err) {
            stop();
        }
        if (!started()) {
            return;
        }
        read_from_input();
    }

    void do_write(const ErrorCode& err, size_t bytes) {
        if (err) {
            stop();
        }
        if (!started()) {
            return;
        }
        primitives::NetworkMessage new_msg = {primitives::Command::CMD_MESSAGE, username_, std::string(input_buffer_, bytes)};
        auto dump = nlohmann::json(new_msg).dump() + "\e";
        sock_.async_write_some( boost::asio::buffer(dump, dump.size()),
                                [shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {shared_this->on_write(err, bytes);});
    }

    size_t read_complete(const boost::system::error_code & err, size_t bytes) {
        if ( err) {
            return 0;
        }
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\e') < read_buffer_ + bytes;
        return found ? 0 : 1;
    }

    size_t input_complete(const boost::system::error_code & err, size_t bytes) {
        if ( err) {
            return 0;
        }
        bool found = std::find(input_buffer_, input_buffer_ + bytes, '\n') < input_buffer_ + bytes;
        return found ? 0 : 1;
    }

    void read_from_input() {
        async_read(input_stream_, boost::asio::buffer(input_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->input_complete(err, bytes);},
                   [shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {shared_this->do_write(err, bytes);});
    }

    void read_from_socket() {
        async_read(sock_, boost::asio::buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {shared_this->on_read(err, bytes);});
    }

private:
    boost::asio::ip::tcp::socket sock_;
    boost::asio::posix::stream_descriptor input_stream_;
    static constexpr int max_msg = 1024;
    char read_buffer_[max_msg];
    char input_buffer_[max_msg];
    bool started_;
    std::string username_;
};

int main(int argc, char* argv[]) {
    auto logger = spdlog::stdout_color_mt("logger");
    boost::asio::io_service service;

    std::string username = primitives::get_user_input(std::cin, std::cout, "Enter your username\n");
    boost::asio::ip::tcp::endpoint ep( boost::asio::ip::address::from_string("127.0.0.1"), 8001);
    clientConnection::create(ep, username, service);

    service.run();
}
