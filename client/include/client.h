#pragma once

#include "deserialize_tools.h"
#include "message.h"
#include "serialize_tools.h"
#include "user_interaction.h"
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

class EasyClient : public std::enable_shared_from_this<EasyClient>, boost::noncopyable {
    explicit EasyClient(std::string& username, boost::asio::io_service& service, std::shared_ptr<spdlog::logger>& logger);
    void start(const boost::asio::ip::tcp::endpoint& ep);

public:
    static std::shared_ptr<EasyClient> create(const boost::asio::ip::tcp::endpoint& ep, std::string& username, boost::asio::io_service& service, std::shared_ptr<spdlog::logger>& logger);
    void stop();
    inline bool started() const { return started_; }

private:
    void on_connect(const boost::system::error_code& err);
    void on_create_sent(const boost::system::error_code& err, size_t bytes);
    void on_create_read(const boost::system::error_code& err, size_t bytes);
    void on_join_sent(const boost::system::error_code& err, size_t bytes);
    void on_join_read(const boost::system::error_code& err, size_t bytes);
    void on_read(const boost::system::error_code& err, size_t bytes);
    void on_write(const boost::system::error_code& err, size_t bytes);
    void do_write(const boost::system::error_code& err, size_t bytes);
    size_t read_complete(const boost::system::error_code& err, size_t bytes);
    size_t input_complete(const boost::system::error_code& err, size_t bytes);
    void read_from_input();
    void read_from_socket();

    boost::asio::ip::tcp::socket sock_;
    boost::asio::posix::stream_descriptor input_stream_;
    static constexpr int max_msg = 1024;
    char read_buffer_[max_msg];
    char input_buffer_[max_msg];
    bool started_;
    std::string username_;
    std::shared_ptr<spdlog::logger>& logger_;
};