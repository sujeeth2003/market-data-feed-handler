CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra
LDFLAGS  ?= -pthread

all: build/test_queues build/bench build/bench_rt build/feed_rx

build:
	mkdir -p build

build/test_queues: tests/test_queues.cpp $(wildcard */*.hpp) | build
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

build/bench: bench/bench.cpp $(wildcard */*.hpp) | build
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

