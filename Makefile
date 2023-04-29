CFLAGS+=-Isrc
CFLAGS+=-DRESOURCEPATH=\"$(out)/src\"
SOURCE=$(wildcard src/*/*.cpp) # doesn't include main.cpp and test.cpp
HEADERS=$(wildcard src/*.h) $(wildcard src/*/*.h)
OBJ=$(patsubst src/%.cpp,build/%.o,$(SOURCE))

all: nix-analyzer nix-analyzer-test parsertest

build/%.o: src/%.cpp $(HEADERS)
	mkdir -p `dirname $@`
	g++ $(CFLAGS) $< -c -o $@

parsertest: ${OBJ} build/parsertest.o
	g++ ${OBJ} build/parsertest.o ${CFLAGS} -o parsertest

nix-analyzer: ${OBJ} build/main.o
	g++ ${OBJ} build/main.o ${CFLAGS} -o nix-analyzer

nix-analyzer-test: ${OBJ} build/test.o
	g++ ${OBJ} build/test.o ${CFLAGS} -lgtest -o nix-analyzer-test

clean:
	rm -rf nix-analyzer nix-analyzer-test parsertest build

install:
	mkdir -p $(out)/bin
	cp -f nix-analyzer $(out)/bin
	cp nix-analyzer-test $(out)
	cp parsertest $(out)
	find src -name '*.nix' -exec cp --parents '{}' $(out) ';'