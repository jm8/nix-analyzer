CFLAGS:=-Isrc -Ibuild -O3 -Wall $(shell pkg-config --cflags --libs bdw-gc nlohmann_json nix-main) -I$(boostInclude) -lnixutil -lnixstore -lnixexpr -lnixmain -lnixcmd -lnixfetchers -lgc
SOURCE:=$(wildcard src/*.cpp)
OBJ:=$(patsubst src/%.cpp,build/%.o,$(SOURCE))

all: nix-analyzer-test

build/%.o: src/%.cpp
	mkdir -p build
	g++ $(CFLAGS) $< -c -o $@

nix-analyzer-test: $(OBJ)
	g++ $(CFLAGS) $(OBJ) -o nix-analyzer-test

nix/config.h:
	cd nix && ./bootstrap.sh && ./configure --prefix=$$PWD/build

clean:
	rm -rf build
	rm nix-analyzer-test
	$(MAKE) -C nix clean