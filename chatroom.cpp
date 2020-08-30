#include <iostream>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include "tcp.hpp"
#include "numeric_type_header.hpp"

class chat_room {
    private:
        using connection_ptr = std::shared_ptr<tcp_connection<chat_room>>;

        std::set<connection_ptr> connections_;

    public:
        // Required for tcp_connection.
        using header = numeric_type_header<std::size_t>;

        void start_connection(connection_ptr conn) {
            connections_.insert(conn);
            std::cout << "Client connected." << std::endl;
        }

        bool read_body(connection_ptr conn, std::vector<char>& message) {
            std::cout.write(message.data(), message.size());
            std::cout << std::endl;
            for (auto other_conn : connections_) {
                if (other_conn != conn) {
                    other_conn->write(message);
                }
            }
            return true;
        }

        // Required for tcp_connection.
        void handle_write_error(connection_ptr conn, const boost::system::error_code& error, const std::vector<char>& message) {
        }

        void close_hook(connection_ptr conn, const boost::system::error_code&) {
            connections_.erase(conn);
            std::cout << "Client disconnected." << std::endl;
        }
};

int main(const int argc, const char* argv[]) {
    if ( argc != 2 ) {
        std::cerr << "You must specify the port for the chatroom to listen on." << std::endl;
        return -1;
    }

    uint16_t port;
    try {
        port = boost::lexical_cast<decltype(port)>(argv[1]);
    } catch (const boost::bad_lexical_cast& ) {
        std::cerr << boost::format("The port must be between %d and %d.")
                % std::numeric_limits<decltype(port)>::min()
                % std::numeric_limits<decltype(port)>::max()
            << std::endl;
        return -1;
    }

    auto chat_application = std::make_shared<chat_room>();
    boost::asio::io_service io_service;
    tcp_server<chat_room> server{io_service, chat_application, port};
    try {
        io_service.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
    return 0;
}
