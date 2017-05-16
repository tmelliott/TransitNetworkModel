CXX := g++
CXXFLAGS := --std=c++11 -g -Wall

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CXXFLAGS += -fopenmp
endif


SRC := src
INCLUDE := include
BUILD := build
BIN := bin

EXT := cpp
INC := -I$(INCLUDE)
LINK := -L/usr/lib/x86_64-linux-gnu:/usr/local/lib
SOURCES := $(shell find $(SRC) -type f -name *.$(EXT))
INCLUDES := $(wildcard $(INCLUDE)/*.h)
OBJECTS := $(patsubst $(SRC)/%, $(BUILD)/%, $(SOURCES:.$(EXT)=.o))
TARGETS := $(patsubst $(SRC)/%, $(BIN)/%, $(SOURCES:.$(EXT)=))

PSOURCE = $(INCLUDE)/gtfs-realtime.pb.cc
POBJECT = $(BUILD)/gtfs-realtime.pb.o
PFLAGS := `pkg-config --cflags --libs protobuf`

all: $(TARGETS)

$(TARGETS): $(OBJECTS) $(INCLUDES) | $(BIN) $(POBJECT)
	@echo "+ Creating $@"
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@ $(LINK)

$(OBJECTS): $(SOURCES) $(INCLUDES) | $(BUILD) $(POBJECT)
	@echo "+ Creating object $@"
	$(CXX) $(CXXFLAGS) $(INC) -c -o $@ $< $(LINK) $(PFLAGS)


## ---- create required directories

directories: $(BUILD) $(BIN)

$(BUILD):
	@mkdir -p $@

$(BIN):
	@mkdir -p $@


## ---- generate the protobuf files

$(POBJECT): $(PSOURCE) | $(BUILD)
	@echo "+ Generating gtfs-realtime.pb object"
	$(CXX) $(CXXFLAGS) $(INC) -c -o $@ $< $(PFLAGS)

$(PSOURCE):
	@echo "+ Generating gtfs-realtime.pb source"
	protoc -I=lib --cpp_out=include lib/gtfs-realtime.proto


## ---- documentation

doc: $(SOURCES) $(INCLUDES)
	doxygen Doxyfile


## ---- clean up

clean:
	@rm -rf build bin $(INCLUDE)/*.pb.* *.pb
