default all:
	g++ -std=c++11 serverA.cpp -o serverA
	g++ -std=c++11 serverB.cpp -o serverB
	g++ -std=c++11 aws.cpp -o aws
	g++ -std=c++11 client.cpp -o client

serverA:
	./serverA

serverB:
	./serverB

aws:
	./aws

.PHONY: serverA serverB aws
