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
SOURCES := $(shell find $(SRC) -type f -name *.$(EXT))
INCLUDES := $(wildcard $(INCLUDE)/*.h)
OBJECTS := $(patsubst $(SRC)/%, $(BUILD)/%, $(SOURCES:.$(EXT)=.o))
TARGETS := $(patsubst $(SRC)/%, $(BIN)/%, $(SOURCES:.$(EXT)=))

all: protobuf $(TARGETS)

$(TARGETS): $(OBJECTS) $(INCLUDES) | $(BIN)
	$(CXX) $(CXXFLAGS) $(INC) $< -o $@

$(OBJECTS): $(SOURCES) $(INCLUDES) | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INC) -c -o $@ $<


## ---- create required directories

directories: $(BUILD) $(BIN)

$(BUILD):
	@mkdir -p $@

$(BIN):
	@mkdir -p $@

## ---- generate the protobuf files

protobuf:
	@echo " TODO: generate protobuf files"


## ---- documentation

doc: $(SOURCES) $(INCLUDES)
	doxygen Doxyfile


## ---- clean up

clean:
	@rm -rf build bin
