all:
	clang++ -g -std=c++2a test.cpp -o test -lboost_context-mt
	
clean:
	rm -f test
	
