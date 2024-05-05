#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>
#include <boost/noncopyable.hpp>
#include <nlohmann/json.hpp>
#include <memory>

using namespace boost::asio;
io_service service;

class talk_to_svr : public boost::enable_shared_from_this<talk_to_svr>
        , boost::noncopyable {
    typedef talk_to_svr self_type;
    talk_to_svr(std::string& username)
            : sock_(service), started_(true), input_stream_(service, STDIN_FILENO), username_(username) {}
    void start(const ip::tcp::endpoint& ep) {
        sock_.async_connect(ep, [shared_this = shared_from_this()](const error_code & err) {
            shared_this->on_connect(err);
        });
    }
public:
    typedef boost::system::error_code error_code;
    typedef std::shared_ptr<talk_to_svr> ptr;

    static ptr create(const ip::tcp::endpoint& ep, std::string& username) {
        ptr new_(std::make_shared<talk_to_svr>(username));
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
        std::cout << "CONNECTED\n";
        std::cout << "Choose option:\n";
        std::cout << "1. Create new room\n";
        std::cout << "2. Join existing room\n";
        int opt;
        std::cin >> opt;
        switch (opt) {
            case 1: {
                nlohmann::json msg = {
                        {"command", "create"},
                        {"user", username_},
                        {"data", ""},
                };
                std::string ll = msg.dump();
                sock_.async_write_some( buffer(msg.dump() + "\e", msg.dump().size() + 1),[shared_this = shared_from_this()](const error_code & err, size_t bytes) {
                    shared_this->on_create_sent(err, bytes);
                });
                break;
            }
            case 2: {
                std::cout << "Enter chat id: ";
                std::string chat_id;
                std::cin >> chat_id;
                nlohmann::json msg = {
                        {"command", "join"},
                        {"user", username_},
                        {"data", chat_id},
                };
                sock_.async_write_some( buffer(msg.dump() + "\e", msg.dump().size() + 1),[shared_this = shared_from_this()](const error_code & err, size_t bytes) {
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
        async_read(sock_, buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_create_read(err, bytes);});
    }

    void on_create_read(const error_code & err, size_t bytes) {
        if (err) stop();
        nlohmann::json answer = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        if (answer["command"] == "create") {
            if (!answer["data"].empty()) {
                std::cout << "New room created: " << answer["data"] << "\n";
                read_from_input();
                read_from_socket();
            } else {
                std::cout << "Failed to create new room\n";
                stop();
            }
        }
    }

    void on_join_sent(const error_code & err, size_t bytes) {
        if (err) stop();
        async_read(sock_, buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_join_read(err, bytes);});
    }

    void on_join_read(const error_code & err, size_t bytes) {
        std::cout << "Join read " + std::string(read_buffer_, bytes - 1) + "\n";
        if (err) stop();
        nlohmann::json answer = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        if (answer["command"] == "join") {
            if (answer["data"] == "success") {
                read_from_input();
                read_from_socket();
            } else {
                std::cout << "Failed to join\n";
                stop();
            }
        }
    }

    void on_read(const error_code & err, size_t bytes) {
        if (err) stop();
        if (!started()) return;
        std::string ans = std::string(read_buffer_, bytes);
        nlohmann::json answer = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));
        std::string ll = answer["data"];
        std::cout << ll;
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
        nlohmann::json msg = {
                {"command", "message"},
                {"user", username_},
                {"data", std::string(input_buffer_, bytes)},
        };
        sock_.async_write_some( buffer(msg.dump() + "\e", msg.dump().size() + 1),
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
        async_read(input_stream_, buffer(input_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->input_complete(err, bytes);},
                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->do_write(err, bytes);});
    }

    void read_from_socket() {
        async_read(sock_, buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code & err, size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_read(err, bytes);});
    }

private:
    ip::tcp::socket sock_;
    posix::stream_descriptor input_stream_;
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
    ip::tcp::endpoint ep( ip::address::from_string("127.0.0.1"), 8001);
    talk_to_svr::create(ep, username);

    service.run();
}
