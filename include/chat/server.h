#pragma once

#include "primitives/common_json.h"
#include "primitives/message.h"
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unordered_map>

class EasyServer : public std::enable_shared_from_this<EasyServer> {
    EasyServer(
        boost::asio::io_service& service,
        std::unordered_map<std::string, std::vector<std::shared_ptr<EasyServer>>>& rooms,
        boost::uuids::random_generator& generator,
        std::shared_ptr<spdlog::logger> logger
    );

public:
    static std::shared_ptr<EasyServer> create(
        boost::asio::io_service& service,
        std::unordered_map<std::string, std::vector<std::shared_ptr<EasyServer>>>& rooms,
        boost::uuids::random_generator& generator,
        std::shared_ptr<spdlog::logger> logger
    );

    void start();

    void stop();

    inline bool getStarted() const { return started_; }

    inline boost::asio::ip::tcp::socket& getSocket() { return sock_; }

    inline std::string getUsername() const { return username_; }

    static void handle_accept(
        const std::shared_ptr<EasyServer>& client,
        [[maybe_unused]] const boost::system::error_code& err,
        boost::asio::ip::tcp::acceptor& acceptor,
        boost::asio::io_service& service,
        std::unordered_map<std::string, std::vector<std::shared_ptr<EasyServer>>>& rooms,
        boost::uuids::random_generator& generator,
        std::shared_ptr<spdlog::logger>& logger
    );

private:
    void on_read(const boost::system::error_code& err, size_t bytes);

    void on_write(const boost::system::error_code& err, [[maybe_unused]] size_t bytes);

    void do_read();

    void do_write(const std::string& msg);

    size_t read_complete(const boost::system::error_code& err, size_t bytes);

private:
    std::string username_;
    std::string room_id_;
    boost::asio::ip::tcp::socket sock_;
    static constexpr int max_msg = 1024;
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<EasyServer>>>& rooms_;
    boost::uuids::random_generator& random_generator_;
    std::shared_ptr<spdlog::logger> logger_;
};

void handle_accept(
    std::shared_ptr<EasyServer> client,
    [[maybe_unused]] const boost::system::error_code& err,
    boost::asio::ip::tcp::acceptor& acceptor,
    boost::asio::io_service& service,
    std::unordered_map<std::string, std::vector<EasyServer>>& rooms,
    boost::uuids::random_generator& generator,
    std::shared_ptr<spdlog::logger>& logger
);
