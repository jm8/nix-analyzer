CFLAGS+=-Isrc
SOURCE=$(wildcard src/*/*.cpp) # doesn't include main.cpp and test.cpp
HEADERS=$(wildcard src/*.h) $(wildcard src/*/*.h)
OBJ=$(patsubst src/%.cpp,build/%.o,$(SOURCE))

all: nix-analyzer nix-analyzer-test

build/%.o: src/%.cpp $(HEADERS)
	mkdir -p `dirname $@`
	g++ $(CFLAGS) $< -c -o $@

nix-analyzer: ${OBJ} build/main.o
	g++ ${OBJ} build/main.o ${CFLAGS} -o nix-analyzer

nix-analyzer-test: ${OBJ} build/test.o
	g++ ${OBJ} build/test.o ${CFLAGS} -lgtest -o nix-analyzer-test

clean:
	rm -rf nix-analyzer nix-analyzer-test build