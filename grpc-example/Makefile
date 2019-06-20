all:
	make proto client server async_server

proto:
	protoc --cpp_out=. my_helloworld.proto
	protoc --grpc_out=. my_helloworld.proto --plugin=protoc-gen-grpc=`which grpc_cpp_plugin`
	
client:
	clang++ -std=c++2a async_client.cpp my_helloworld.grpc.pb.cc my_helloworld.pb.cc -o client -I../ -lprotobuf -lgrpc -lgrpc++ -lboost_context-mt

server:
	clang++ -std=c++2a server.cpp my_helloworld.grpc.pb.cc my_helloworld.pb.cc -o server -I../ -lprotobuf -lgrpc -lgrpc++ -lboost_context-mt
	
async_server:
	clang++ -g -std=c++2a async_server.cpp my_helloworld.grpc.pb.cc my_helloworld.pb.cc -o async_server -I../ -lprotobuf -lgrpc -lgrpc++ -lboost_context-mt

clean:
	rm -f *.pb.h *.pb.cc client server async_server
