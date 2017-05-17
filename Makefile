default:
	mkdir -p build
	cd protobuf && protoc --cpp_out=. gtfs-realtime.proto && mv *.cc ../src/gtfs-realtime.pb.cpp && mv *.h ../include/gtfs-realtime.pb.h
	cd build && cmake ..
