all: rpc dnode hub

rpc:
	g++ src/rpc.cpp `find src/util -name "*.cpp"` -o bin/rpc

dnode:
	g++ src/dnode.cpp `find src/util src/dnode -name "*.cpp"` -pthread -o bin/dnode

hub:
	g++ src/hub.cpp `find src/util src/hub -name "*.cpp"` -pthread -o bin/hub
