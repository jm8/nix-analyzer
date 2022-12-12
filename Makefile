CFLAGS:=-Isrc -Ibuild -O3 -Wall -std=c++20 $(shell pkg-config --cflags --libs bdw-gc nlohmann_json) -I$(boostInclude) -Inix/build/include/nix -Inix -Lnix/build/lib -lnixutil -lnixstore -lnixexpr -lnixmain -lnixcmd -lnixfetchers -lgc
SOURCE:=$(wildcard src/*.cpp)
OBJ:=$(patsubst src/%.cpp,build/%.o,$(SOURCE))

all: nix-analyzer

build/%.o: src/%.cpp
	mkdir -p build
	g++ $(CFLAGS) $< -c -o $@

nix-analyzer: $(OBJ) nixlib
	g++ $(CFLAGS) $(OBJ) -o nix-analyzer

nixlib:
	$(MAKE) -C nix

clean:
	rm -rf build
	rm nix-analyzer