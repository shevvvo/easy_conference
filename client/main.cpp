#include "deserialize_tools.h"
#include "message.h"
#include "serialize_tools.h"
#include "user_interaction.h"
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

class ClientConnection
    : public std::enable_shared_from_this<ClientConnection>
    , boost::noncopyable {
    using ErrorCode = boost::system::error_code;
    using ClientPtr = std::shared_ptr<ClientConnection>;
    using Logger = std::shared_ptr<spdlog::logger>;

    explicit ClientConnection(std::string& username, boost::asio::io_service& service, Logger& logger)
        : sock_(service), input_stream_(service, STDIN_FILENO), started_(true), username_(username), logger_(logger) {}

    void start(const boost::asio::ip::tcp::endpoint& ep) {
        sock_.async_connect(ep, [shared_this = shared_from_this()](const ErrorCode& err) {
            shared_this->on_connect(err);
        });
    }

public:
    static ClientPtr create(
        const boost::asio::ip::tcp::endpoint& ep,
        std::string& username,
        boost::asio::io_service& service,
        Logger& logger
    ) {
        ClientPtr new_(new ClientConnection(username, service, logger));
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
        logger_->info("Connected");
        primitives::Command opt = primitives::get_user_command(
            std::cin, std::cout, "Choose option:\n1. Create new room\n2. Join existing room\n"
        );
        switch (opt) {
        case primitives::Command::CMD_CREATE: {
            auto req = primitives::serialize_json({ primitives::Command::CMD_CREATE, username_, "" });
            sock_.async_write_some(
                boost::asio::buffer(req, req.size()),
                [shared_this = shared_from_this()](const ErrorCode& err_, size_t bytes) {
                    shared_this->on_create_sent(err_, bytes);
                }
            );
            break;
        }
        case primitives::Command::CMD_JOIN: {
            std::string chat_id = primitives::get_user_input(std::cin, std::cout, "Enter chat id: ");
            auto req = primitives::serialize_json({ primitives::Command::CMD_JOIN, username_, chat_id });
            sock_.async_write_some(
                boost::asio::buffer(req, req.size()),
                [shared_this = shared_from_this()](const ErrorCode& err_, size_t bytes) {
                    shared_this->on_join_sent(err_, bytes);
                }
            );
            break;
        }
        default: {
            break;
        }
        }
    }

    void on_create_sent(const ErrorCode& err, [[maybe_unused]] size_t bytes) {
        if (err) {
            stop();
        }
        async_read(
            sock_,
            boost::asio::buffer(read_buffer_),
            [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
                return shared_this->read_complete(err_, bytes_);
            },
            [shared_this = shared_from_this()](const ErrorCode& err_, size_t bytes_) {
                shared_this->on_create_read(err_, bytes_);
            }
        );
    }

    void on_create_read(const ErrorCode& err, size_t bytes) {
        if (err) {
            stop();
        }
        auto new_msg = primitives::deserialize_json(std::string(read_buffer_, bytes - 1));
        if (new_msg.command == primitives::Command::CMD_CREATE) {
            if (!new_msg.data.empty()) {
                logger_->info("New room created: " + new_msg.data);
                read_from_input();
                read_from_socket();
            } else {
                logger_->info("Failed to create new room");
                stop();
            }
        }
    }

    void on_join_sent(const ErrorCode& err, [[maybe_unused]] size_t bytes) {
        if (err) {
            stop();
        }
        async_read(
            sock_,
            boost::asio::buffer(read_buffer_),
            [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
                return shared_this->read_complete(err_, bytes_);
            },
            [shared_this = shared_from_this()](const ErrorCode& err_, size_t bytes_) {
                shared_this->on_join_read(err_, bytes_);
            }
        );
    }

    void on_join_read(const ErrorCode& err, size_t bytes) {
        logger_->info("Join read " + std::string(read_buffer_, bytes - 1));
        if (err) {
            stop();
        }
        auto new_msg = primitives::deserialize_json(std::string(read_buffer_, bytes - 1));
        if (new_msg.command == primitives::Command::CMD_JOIN) {
            if (new_msg.data == "success") {
                read_from_input();
                read_from_socket();
            } else {
                logger_->info("Failed to join");
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
        logger_->info(std::string(read_buffer_, bytes - 1));
        read_from_socket();
    }

    void on_write(const ErrorCode& err, [[maybe_unused]] size_t bytes) {
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
        auto req = primitives::serialize_json(
            { primitives::Command::CMD_MESSAGE, username_, std::string(input_buffer_, bytes) }
        );
        sock_.async_write_some(
            boost::asio::buffer(req, req.size()),
            [shared_this = shared_from_this()](const ErrorCode& err_, size_t bytes_) {
                shared_this->on_write(err_, bytes_);
            }
        );
    }

    size_t read_complete(const boost::system::error_code& err, size_t bytes) {
        if (err) {
            return 0;
        }
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\r') < read_buffer_ + bytes;
        return found ? 0 : 1;
    }

    size_t input_complete(const boost::system::error_code& err, size_t bytes) {
        if (err) {
            return 0;
        }
        bool found = std::find(input_buffer_, input_buffer_ + bytes, '\n') < input_buffer_ + bytes;
        return found ? 0 : 1;
    }

    void read_from_input() {
        async_read(
            input_stream_,
            boost::asio::buffer(input_buffer_),
            [shared_this = shared_from_this()](const boost::system::error_code& err, size_t bytes) {
                return shared_this->input_complete(err, bytes);
            },
            [shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) {
                shared_this->do_write(err, bytes);
            }
        );
    }

    void read_from_socket() {
        async_read(
            sock_,
            boost::asio::buffer(read_buffer_),
            [shared_this = shared_from_this()](const boost::system::error_code& err, size_t bytes) {
                return shared_this->read_complete(err, bytes);
            },
            [shared_this = shared_from_this()](const ErrorCode& err, size_t bytes) { shared_this->on_read(err, bytes); }
        );
    }

private:
    boost::asio::ip::tcp::socket sock_;
    boost::asio::posix::stream_descriptor input_stream_;
    static constexpr int max_msg = 1024;
    char read_buffer_[max_msg];
    char input_buffer_[max_msg];
    bool started_;
    std::string username_;
    Logger& logger_;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    auto logger = spdlog::stdout_color_mt("logger");
    boost::asio::io_service service;

    std::string username = primitives::get_user_input(std::cin, std::cout, "Enter your username\n");
    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("127.0.0.1"), 8001);
    ClientConnection::create(ep, username, service, logger);

    service.run();
}
