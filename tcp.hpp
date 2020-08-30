#include <memory>
#include <queue>
#include <set>
#include <mutex>

#include <boost/optional.hpp>
#include <boost/asio.hpp>

/**
 * To use this header, you will need an application (a class) designed for it.
 * The class must implement the following methods:
 *      void start_connection(shared_ptr<tcp_connection<T> conn);
 *      bool read_body( shared_ptr<tcp_connection<T> conn,
 *                      std::vector<char>& message);
 *      void handle_write_error(shared_ptr<tcp_connection<T> conn,
 *                              const boost::system::error_code& error,
 *                              std::vector<char>& message);
 *      void close_hook(shared_ptr<tcp_connection<T> conn,
 *                      const boost::system::error_code& error);
 * It must also define a type "header" that has a "length" attribute and the following static functions:
 *      header::type decode_header(std::array<unsigned_char, header::length>& header);
 *      std::array<unsigned_char, header::length> decode_header(header::type body_length);
 */

template<typename T>
class tcp_connection : public std::enable_shared_from_this<tcp_connection<T>> {
    public:
        static auto create(boost::asio::io_service& service, std::shared_ptr<T> application) {
            //construct shared_ptr instead of using make_shared to use private constructor
            return std::shared_ptr<tcp_connection>{new tcp_connection{service, application}};
        }

        auto& socket() { return socket_; }

        void start() {
            application_->start_connection(this->shared_from_this());
            do_read_header();
        }

        template<typename CharIterable>
        void write(const CharIterable& write_body) {
            auto self{this->shared_from_this()};
            socket_.get_io_service().post(
                    [this, self, write_body]()
                    {
                        auto write_message_header = T::header::encode_header(write_body.size());
                        std::vector<char> write_message{write_message_header.cbegin(), write_message_header.cend()};
                        write_message.insert(write_message.cend(), write_body.cbegin(), write_body.cend());
                        {
                            std::lock_guard<std::mutex> lock{write_messages_mutex_};
                            write_messages_.push(std::move(write_message));
                        }
                        do_write();
                    });
        }

        void close(const boost::system::error_code& error) {
            auto self{this->shared_from_this()};
            socket_.get_io_service().post(
                    [this, self, error]()
                    {
                        application_->close_hook(self, error);
                        socket_.close();
                    });
        }

    private:
        tcp_connection(boost::asio::io_service& service, std::shared_ptr<T> application) :
            socket_{service},
            application_{application}
        {}

        void do_read_header() {
            auto self{this->shared_from_this()};
            boost::asio::async_read(socket_,
                    boost::asio::buffer(read_header_.data(), T::header::length),
                    [this, self](const boost::system::error_code& error, const std::size_t /*length*/)
                    {
                        if (error) {
                            close(error);
                        }
                        else {
                            auto body_length = T::header::decode_header(read_header_);
                            if (body_length != -1) {
                                read_message_.resize(body_length);
                                do_read_body();
                            } else {
                                close(boost::system::error_code{});
                            }
                        }
                    });
        }

        void do_read_body() {
            auto self{this->shared_from_this()};
            boost::asio::async_read(socket_,
                    boost::asio::buffer(read_message_.data(), read_message_.size()),
                    [this, self](const boost::system::error_code& error, const std::size_t /*length*/)
                    {
                        if (error) {
                            close(error);
                        }
                        else if (application_->read_body(self, read_message_)) {
                            do_read_header();
                        } else {
                            close(boost::system::error_code{});
                        }
                    });
        }

        void do_write() {
            auto message = [this]() -> boost::optional<std::vector<char>> {
                std::lock_guard<std::mutex> lock{write_messages_mutex_};
                if (write_messages_.empty()) {
                    return {};
                }
                auto message = write_messages_.front();
                write_messages_.pop();
                return message;
            }();
            if (!message) {
                return;
            }
            auto self{this->shared_from_this()};
            boost::asio::async_write(socket_,
                    boost::asio::buffer(message->data(), message->size()),
                    [this, self, message](boost::system::error_code error, std::size_t /*length*/)
                    {
                        if (error) {
                            application_->handle_write_error(self, error, *message);
                            close(error);
                        } else {
                            do_write();
                        }
                    });
        }

        boost::asio::ip::tcp::socket socket_;
        std::array<unsigned char, T::header::length> read_header_;
        std::vector<char> read_message_;
        std::queue<std::vector<char>> write_messages_;
        std::mutex write_messages_mutex_;
        std::shared_ptr<T> application_;
};

template<typename T>
class tcp_server {
    public:
        tcp_server(boost::asio::io_service& service, std::shared_ptr<T> application, const uint16_t port)
            : acceptor_{service, boost::asio::ip::tcp::endpoint{boost::asio::ip::tcp::v4(), port}},
            application_{application}
        {
            start_accept();
        }

        virtual ~tcp_server() = default;

    private:
        void start_accept() {
            auto new_connection = tcp_connection<T>::create(acceptor_.get_io_service(), application_);

            acceptor_.async_accept(new_connection->socket(),
                    [this, new_connection](const boost::system::error_code& error)
                    {
                        if (!error)
                        {
                            new_connection->start();
                        }
                        start_accept();
                    });
        }

        boost::asio::ip::tcp::acceptor acceptor_;
        std::shared_ptr<T> application_;
};

template<typename T>
class tcp_client {
    private:
        boost::asio::ip::tcp::resolver endpoint_resolver_;
        std::shared_ptr<T> application_;

    public:
        tcp_client(boost::asio::io_service& service, std::shared_ptr<T> application, const std::string& server, const std::string& service_name_or_port)
            : endpoint_resolver_{service},
            application_{application}
        {
            connect(server, service_name_or_port);
        }

        virtual ~tcp_client() = default;

    private:
        void connect(const std::string& server, const std::string& service_name_or_port) {
            auto new_connection = tcp_connection<T>::create(endpoint_resolver_.get_io_service(), application_);

            boost::asio::async_connect(new_connection->socket(),
                    endpoint_resolver_.resolve(boost::asio::ip::tcp::resolver::query{server, service_name_or_port}),
                    [this, new_connection](const boost::system::error_code& error, boost::asio::ip::tcp::resolver::iterator)
                    {
                        if (!error) {
                            new_connection->start();
                        }
                    });
        }
};

