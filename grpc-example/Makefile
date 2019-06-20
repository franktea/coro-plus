proto:
	protoc --cpp_out=. my_helloworld.proto
	protoc --grpc_out=. my_helloworld.proto --plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin
	
client:
	clang++ -std=c++2a async_client.cpp my_helloworld.grpc.pb.cc my_helloworld.pb.cc -o client -I../ -lprotobuf -lgrpc -lgrpc++ -lboost_context-mt

	