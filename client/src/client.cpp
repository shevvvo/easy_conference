#include "client.h"

EasyClient::EasyClient(std::string& username, boost::asio::io_service& service, std::shared_ptr<spdlog::logger>& logger)
    : sock_(service), input_stream_(service, STDIN_FILENO), started_(true), username_(username), logger_(logger) {}

void EasyClient::start(const boost::asio::ip::tcp::endpoint& ep) {
    sock_.async_connect(ep, [shared_this = shared_from_this()](const boost::system::error_code& err) {
        shared_this->on_connect(err);
    });
}

std::shared_ptr<EasyClient> EasyClient::create(const boost::asio::ip::tcp::endpoint& ep, std::string& username, boost::asio::io_service& service, std::shared_ptr<spdlog::logger>& logger) {
    std::shared_ptr<EasyClient> new_(new EasyClient(username, service, logger));
    new_->start(ep);
    return new_;
}

void EasyClient::stop() {
    if (!started_) {
        return;
    }
    started_ = false;
    sock_.close();
}

void EasyClient::on_connect(const boost::system::error_code& err) {
    if (err) {
        stop();
    }
    logger_->info("Connected");
    primitives::Command opt{};
    primitives::get_user_input(
        std::cin, std::cout, "Choose option:\n1. Create new room\n2. Join existing room\n", opt
    );
    switch (opt) {
    case primitives::Command::CMD_CREATE: {
        auto req = primitives::serialize_json({ primitives::Command::CMD_CREATE, username_, "" });
        sock_.async_write_some(
            boost::asio::buffer(req, req.size()),
            [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes) {
                shared_this->on_create_sent(err_, bytes);
            }
        );
        break;
    }
    case primitives::Command::CMD_JOIN: {
        std::string chat_id{};
        primitives::get_user_input(std::cin, std::cout, "Enter chat id: ", chat_id);
        auto req = primitives::serialize_json({ primitives::Command::CMD_JOIN, username_, chat_id });
        sock_.async_write_some(
            boost::asio::buffer(req, req.size()),
            [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes) {
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

void EasyClient::on_create_sent(const boost::system::error_code& err, [[maybe_unused]] size_t bytes) {
    if (err) {
        stop();
    }
    async_read(
        sock_,
        boost::asio::buffer(read_buffer_),
        [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
            return shared_this->read_complete(err_, bytes_);
        },
        [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
            shared_this->on_create_read(err_, bytes_);
        }
    );
}

void EasyClient::on_create_read(const boost::system::error_code& err, size_t bytes) {
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

void EasyClient::on_join_sent(const boost::system::error_code& err, [[maybe_unused]] size_t bytes) {
    if (err) {
        stop();
    }
    async_read(
        sock_,
        boost::asio::buffer(read_buffer_),
        [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
            return shared_this->read_complete(err_, bytes_);
        },
        [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
            shared_this->on_join_read(err_, bytes_);
        }
    );
}

void EasyClient::on_join_read(const boost::system::error_code& err, size_t bytes) {
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

void EasyClient::on_read(const boost::system::error_code& err, size_t bytes) {
    if (err) {
        stop();
    }
    if (!started()) {
        return;
    }
    logger_->info(std::string(read_buffer_, bytes - 1));
    read_from_socket();
}

void EasyClient::on_write(const boost::system::error_code& err, [[maybe_unused]] size_t bytes) {
    if (err) {
        stop();
    }
    if (!started()) {
        return;
    }
    read_from_input();
}

void EasyClient::do_write(const boost::system::error_code& err, size_t bytes) {
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
        [shared_this = shared_from_this()](const boost::system::error_code& err_, size_t bytes_) {
            shared_this->on_write(err_, bytes_);
        }
    );
}

size_t EasyClient::read_complete(const boost::system::error_code& err, size_t bytes) {
    if (err) {
        return 0;
    }
    bool found = std::find(read_buffer_, read_buffer_ + bytes, '\n') < read_buffer_ + bytes;
    return found ? 0 : 1;
}

size_t EasyClient::input_complete(const boost::system::error_code& err, size_t bytes) {
    if (err) {
        return 0;
    }
    bool found = std::find(input_buffer_, input_buffer_ + bytes, '\n') < input_buffer_ + bytes;
    return found ? 0 : 1;
}

void EasyClient::read_from_input() {
    async_read(
        input_stream_,
        boost::asio::buffer(input_buffer_),
        [shared_this = shared_from_this()](const boost::system::error_code& err, size_t bytes) {
            return shared_this->input_complete(err, bytes);
        },
        [shared_this = shared_from_this()](const boost::system::error_code& err, size_t bytes) {
            shared_this->do_write(err, bytes);
        }
    );
}

void EasyClient::read_from_socket() {
    async_read(
        sock_,
        boost::asio::buffer(read_buffer_),
        [shared_this = shared_from_this()](const boost::system::error_code& err, size_t bytes) {
            return shared_this->read_complete(err, bytes);
        },
        [shared_this = shared_from_this()](const boost::system::error_code& err, size_t bytes) { shared_this->on_read(err, bytes); }
    );
}