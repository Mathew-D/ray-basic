CC      ?= cc
CXX     ?= c++
CFLAGS ?= -O2 -g
CXXFLAGS ?= -Wall -Wextra -O2 -g -std=c++23
LDLIBS  := $(shell pkg-config --libs raylib)
CINCS   := $(shell pkg-config --cflags raylib)

.DEFAULT_GOAL := all

TARGET  := game
SRCS    := $(wildcard src/*.cpp)
DEPS    := $(wildcard src/*.h)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) $(DEPS)
	$(CXX) $(CXXFLAGS) $(CINCS) -o $@ $(SRCS) $(LDLIBS) -lm -lpthread -ldl -lrt

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
