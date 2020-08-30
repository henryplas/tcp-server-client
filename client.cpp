#include <iostream>
#include <string>
#include <thread>
#include <future>

#include "tcp.hpp"
#include "numeric_type_header.hpp"

class chat_client {
    private:
        using connection_ptr = std::shared_ptr<tcp_connection<chat_client>>;

        std::promise<connection_ptr> connection_promise_;
        std::shared_future<connection_ptr> connection_{connection_promise_.get_future()};

    public:
        // Required by tcp_connection.
        using header = numeric_type_header<std::size_t>;

        void start_connection(connection_ptr conn) {
            connection_promise_.set_value(conn);
        }

        bool read_body(connection_ptr conn, std::vector<char>& message) {
            std::cout.write(message.data(), message.size());
            std::cout << std::endl;
            return true;
        }

        // Required by tcp_connection.
        void handle_write_error(connection_ptr conn,
                                const boost::system::error_code& error,
                                const std::vector<char>& message)
        {}

        void close_hook(connection_ptr conn, const boost::system::error_code& error) {
            if (error) {
                std::cout << "Disconnected from session by host." << std::endl;
            }
        }

        bool send(const std::string& message) {
            auto conn = connection_.get();
            if ( !conn ) {
                return false;
            }
            conn->write(message);
            return true;
        }

        bool close() {
            auto conn = connection_.get();
            if ( !conn ) {
                return false;
            }
            conn->close(boost::system::error_code{});
            return true;
        }
};

int main(const int argc, const char* argv[]) {

    if ( argc != 3 ) {
        std::cerr << "You must specify the host and service or port for the client to connect to." << std::endl;
        return -1;
    }

    auto host = argv[1];
    auto service_or_port = argv[2];

    try {
        auto chat_application = std::make_shared<chat_client>();
        boost::asio::io_service io_service;
        tcp_client<chat_client> client{io_service, chat_application, host, service_or_port};
        std::thread t{[&io_service](){ io_service.run(); }};

        std::string line;
        while (std::getline(std::cin, line))
        {
            chat_application->send(line);
        }

        chat_application->close();
        t.join();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return -1;
    }

    return 0;
}
