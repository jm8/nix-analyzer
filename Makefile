CFLAGS+=-Isrc -Itest
CFLAGS+=-DRESOURCEPATH=\"$(out)/src\"
SOURCE=$(wildcard src/*/*.cpp) # doesn't include main.cpp
HEADERS=$(wildcard src/*.h) $(wildcard src/*/*.h)
OBJ=$(patsubst src/%.cpp,build/%.o,$(SOURCE))
TESTOBJ=$(patsubst test/test/%.cpp,build/test/%.o,$(wildcard test/tests/*.cpp)) # doesn't include test.cpp or parsertest.cpp

all: nix-analyzer nix-analyzer-test parsertest

build/%.o: src/%.cpp $(HEADERS)
	mkdir -p `dirname $@`
	g++ $(CFLAGS) $< -c -o $@

build/test/%.o: test/%.cpp $(HEADERS)
	mkdir -p `dirname $@`
	g++ $(CFLAGS) $< -c -o $@

parsertest: ${OBJ} build/test/parsertest.o
	g++ ${OBJ} build/test/parsertest.o ${CFLAGS} -o $@

nix-analyzer: ${OBJ} build/main.o
	g++ ${OBJ} build/main.o ${CFLAGS} -o $@

nix-analyzer-test: ${OBJ} ${TESTOBJ} build/test/test.o
	g++ ${OBJ} ${TESTOBJ} build/test/test.o ${CFLAGS} -o $@

clean:
	rm -rf $(out) nix-analyzer nix-analyzer-test parsertest build

install: nix-analyzer-test src
	mkdir -p $(out)/bin
	cp -f nix-analyzer $(out)/bin
	cp nix-analyzer-test $(out)
	cp parsertest $(out)
	find src -name '*.nix' -exec cp --parents '{}' $(out) ';'