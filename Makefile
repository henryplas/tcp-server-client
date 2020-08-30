all: chatroom client chatbot
chatroom: chatroom.cpp tcp.hpp numeric_type_header.hpp
	g++ -std=c++14 chatroom.cpp -lboost_system -lpthread -o chatroom
client: client.cpp tcp.hpp numeric_type_header.hpp
	g++ -std=c++14 client.cpp -lboost_system -lpthread -o client
chatbot: client.cpp tcp.hpp numeric_type_header.hpp
	g++ -std=c++14 chatbot.cpp -lboost_system -lpthread -o chatbot
clean:
	rm chatroom
	rm client
	rm chatbot
