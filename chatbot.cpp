#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <string>

#include "tcp.hpp"
#include "numeric_type_header.hpp"

class chat_bot {
    private:
        using connection_ptr = std::shared_ptr<tcp_connection<chat_bot>>;

        std::promise<connection_ptr> connection_promise_;
        std::shared_future<connection_ptr> connection_{connection_promise_.get_future()};
        
        std::queue<std::string> chat_queue;

    public:
        // Required by tcp_connection.
        using header = numeric_type_header<std::size_t>;
        
        void start_connection(connection_ptr conn) {
            connection_promise_.set_value(conn);
        }

        bool read_body(connection_ptr conn, std::vector<char>& message) {
            std::cout.write(message.data(), message.size());
            std::cout << std::endl;
            std::string message_to_string(message.begin(), message.end());
            response(message_to_string);
            return true;
        }

        void response(std::string message)
        {
            auto pos = message.find_first_not_of(' ');
            auto trimmed = message.substr(pos != std::string::npos ? pos : 0);
            auto pos_of_first_space = trimmed.find(" ");
            std::string first_word = trimmed.substr(0, pos_of_first_space);
            std::string the_rest = trimmed.substr(pos_of_first_space + 1);

            if (first_word == "!echo")
            {
                send(the_rest);
            }
            else if (first_word == "!eval")
            {
                try
                {
                    int answer = calculate(the_rest);
                    send(std::to_string(answer));
                }
                catch(...)
                {
                    send("Invalid input to eval");
                }
                
            }
        }

        // considering the format of the expression stays the same as the task.txt suggests,
        // this function will solve "Number [space] Operator [space] Number"
        // everything else is caught by try/catch
        int calculate(std::string message)
        {
            auto pos_of_first_space = message.find(' ');
            
            std::string first_number = message.substr(0, pos_of_first_space);
            auto pos_of_space_before_second = pos_of_first_space + 2;
            char operator_char = message.at(pos_of_first_space + 1);
            std::string second_number = message.substr(pos_of_space_before_second + 1);

            int first_int = std::stoi(first_number);
            int second_int = std::stoi(second_number);
            int return_val = 0;

            if(operator_char == '+')
            {
                return_val = first_int + second_int;
            }
            else if(operator_char == '-')
            {
                return_val = first_int - second_int;
            }
            else if(operator_char == '/')
            {
                return_val = first_int / second_int;
            }
            else if(operator_char == '*')
            {
                return_val = first_int * second_int;
            }
            else
            {
                throw "Bad operator";
            }
            
            return return_val;
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
        std::cerr << "You must specify the host and service or port for the chatbot to connect to." << std::endl;
        return -1;
    }

    auto host = argv[1];
    auto service_or_port = argv[2];

    try {
        auto bot_application = std::make_shared<chat_bot>();
        boost::asio::io_service io_service;
        tcp_client<chat_bot> client{io_service, bot_application, host, service_or_port};
        std::thread t{[&io_service](){ io_service.run(); }};


        std::string line;
        while (std::getline(std::cin, line))
        {
            // bot_application->send(line);
        }

        bot_application->close();
        t.join();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return -1;
    }

    return 0;
}
