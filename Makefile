default: build
	cd build && cmake ..

build:
	mkdir -p build

FILES := $(shell find . -maxdepth 2 -type f -name "*h" -o -name "*.cpp")
docs: $(FILES)
	doxygen Doxyfile

clean:
	rm -rf build
