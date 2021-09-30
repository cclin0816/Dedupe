CXX = clang++
CXXFLAGS = --std=c++20 -Wall -Wextra -Wpedantic -O3 -pthread -lcrypto

.PHONY: clean

all: dedupe

dedupe: main.cpp dedupe.o
	$(CXX) $(CXXFLAGS) main.cpp dedupe.o -o dedupe

dedupe.o: dedupe.cpp
	$(CXX) $(CXXFLAGS) -c dedupe.cpp -o dedupe.o

clean:
	rm dedupe *.o 