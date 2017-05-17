default:
	mkdir -p build
	cd protobuf && protoc --cpp_out=. gtfs-realtime.proto
	cd build && cmake ..

clean:
	rm -rf build
