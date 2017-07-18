
default: build
	cd build && cmake ..

build: protofiles
	mkdir -p build

FILES := $(shell find . -maxdepth 2 -type f -name "*h" -o -name "*.cpp")
docs: $(FILES)
	doxygen Doxyfile

clean:
	rm -rf build *.pb

protofiles:
	cd protobuf && ./build_proto_files

protoc: build/protobuf
	sudo apt-get install autoconf automake libtool curl make g++ unzip
	cd $< && ./autogen.sh && ./configure && make && sudo make install && sudo ldconfig

build/protobuf: build/protobuf.zip
	unzip $^ -d build && mv build/*-master $@ && rm $<

build/protobuf.zip: build
	wget https://github.com/google/protobuf/archive/master.zip -O $@



data: gtfs.zip
	unzip -o $< -d $@

gtfs.zip:
	curl -S https://cdn01.at.govt.nz/data/gtfs.zip -o gtfs.zip
