#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp> 
#include <iostream>
#include <boost/noncopyable.hpp>
#include <sstream>
#include <nlohmann/json.hpp>
#include <unordered_map>

using namespace boost::asio;
io_service service {};

class talk_to_client;
typedef boost::shared_ptr<talk_to_client> client_ptr;
typedef std::vector<client_ptr> array;
std::unordered_map<std::string, array> rooms_;

class talk_to_client : public boost::enable_shared_from_this<talk_to_client>
        , boost::noncopyable {
    typedef talk_to_client self_type;
    talk_to_client() : sock_(service), started_(false) {
    }
public:
    typedef boost::system::error_code error_code;
    typedef boost::shared_ptr<talk_to_client> ptr;

    void start() {
        started_ = true;
        do_read();
    }
    static ptr new_() {
        ptr new_client(new talk_to_client);
        return new_client;
    }
    void stop() {
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
    ip::tcp::socket & sock() { return sock_;}
    std::string getUsername() const { return username_; }
private:
    void on_read(const error_code & err, size_t bytes) {
        if ( err) stop();
        if ( !started() ) return;
        if (!bytes) return;
        nlohmann::json request = nlohmann::json::parse(std::string(read_buffer_, bytes - 1));

        if (request["command"] == "join") {
            std::string id = request["data"];
            auto it = rooms_.find(id);
            if (it != rooms_.end()) {
                room_id_ = id;
                it->second.push_back(shared_from_this());
                nlohmann::json answer = {
                        {"command", "join"},
                        {"data", "success"}
                };
                username_ = request["user"];
                sock_.async_write_some(buffer(answer.dump() + "\e", answer.dump().size() + 1),
                                       [shared_this = shared_from_this()](const error_code &err, size_t bytes) {shared_this->on_write(err, bytes);});

            } else {
                nlohmann::json answer = {
                        {"command", "join"},
                        {"data", "fail"}
                };
                sock_.async_write_some(buffer(answer.dump() + "\e", answer.dump().size() + 1),
                                       [shared_this = shared_from_this()](const error_code &err, size_t bytes) {shared_this->on_write(err, bytes);});
            }
        } else if (request["command"] == "create") {
            boost::uuids::uuid uuid = boost::uuids::random_generator()();
            room_id_ = boost::uuids::to_string(uuid);
            array vec = {shared_from_this()};
            rooms_[room_id_] = vec;
            nlohmann::json answer = {
                    {"command", "create"},
                    {"data", room_id_}
            };
            username_ = request["user"];
            sock_.async_write_some(buffer(answer.dump() + "\e", answer.dump().size() + 1),
                                   [shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_write(err, bytes);});
        } else {
            if (!room_id_.empty()) {
                auto it = rooms_.find(room_id_);
                if (it != rooms_.end()) {
                    std::string strr = std::string(read_buffer_, bytes);
                    for (auto& elem : it->second) {
                        std::string nmm = elem->getUsername();
                        if (elem->getUsername() != username_) {
                            std::string ansss = request.dump();
                            std::string kekk("\e");
                            auto llll = kekk.size();
                            elem->sock().async_write_some(buffer(ansss + "\e", ansss.size() + 1),
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
        std::cout << "On write\n";
        //do_read();
    }
    void do_read() {
        async_read(sock_, buffer(read_buffer_), [shared_this = shared_from_this()](const boost::system::error_code& err, std::size_t bytes) {return shared_this->read_complete(err, bytes);},
                   [shared_this = shared_from_this()](const boost::system::error_code& err, std::size_t bytes) {shared_this->on_read(err, bytes);});
    }
    void do_write(const std::string & msg) {
        if ( !started() ) return;
        std::copy(msg.begin(), msg.end(), write_buffer_);
        sock_.async_write_some( buffer(write_buffer_, msg.size()),[shared_this = shared_from_this()](const error_code & err, size_t bytes) {shared_this->on_write(err, bytes);});
    }
    size_t read_complete(const boost::system::error_code & err, size_t bytes) {
        if ( err) {
            return 0;
        }
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\e') < read_buffer_ + bytes;
        // we read one-by-one until we get to enter, no buffering
        return found ? 0 : 1;
    }
private:
    std::string username_;
    std::string room_id_;
    ip::tcp::socket sock_;
    enum { max_msg = 1024 };
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
};

ip::tcp::acceptor acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 8001));

void handle_accept(talk_to_client::ptr client, const boost::system::error_code & err) {
    client->start();
    talk_to_client::ptr new_client = talk_to_client::new_();
    acceptor.async_accept(new_client->sock(), [client = new_client](const boost::system::error_code & err){ handle_accept(client, err);});
}

int main(int argc, char* argv[]) {
    talk_to_client::ptr client = talk_to_client::new_();
    acceptor.async_accept(client->sock(), [client = client](const boost::system::error_code & err){ handle_accept(client, err);});
    service.run();
}
