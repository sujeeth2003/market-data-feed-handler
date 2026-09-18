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

# same benchmark plus the v6 huge-page / mlock variant
build/bench_rt: bench/bench.cpp $(wildcard */*.hpp) | build
	$(CXX) $(CXXFLAGS) -DWITH_RT $< -o $@ $(LDFLAGS)

# Linux only
build/feed_rx: v5_af_packet/feed_rx.cpp $(wildcard common/*.hpp) | build
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

test: build/test_queues
	./build/test_queues

bench: build/bench
	./build/bench 1 500000 1000 0 && ./build/bench 3 300000 1500 0

# Linux, root: AF_PACKET PACKET_MMAP demo on loopback
rx: build/feed_rx
	sudo ./build/feed_rx lo 15000 200000 2000

clean:
	rm -rf build
.PHONY: all test bench rx clean
