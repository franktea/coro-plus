all:
	make client server

client:
	clang++ -g -std=c++2a echo-client.cpp -o echo-client -I../ -I./asio/asio/include -lboost_context-mt

server:
	clang++ -g -std=c++2a echo-server.cpp -o echo-server -I../ -I./asio/asio/include -lboost_context-mt

clean:
	rm -f echo-client echo-server