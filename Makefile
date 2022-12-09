CFLAGS:=-Isrc -O3 -Wall -std=c++20 $(shell pkg-config --cflags --libs nix-main bdw-gc nlohmann_json) -I$(boostInclude) -DNIX_VERSION=\"$(NIX_VERSION)\" -lnixutil -lnixstore -lnixexpr -lnixmain -lnixcmd -lnixfetchers -lgc
SOURCE:=$(wildcard src/*.cpp) build/lexer-tab.cpp build/parser-tab.cpp
OBJ:=$(patsubst build/%.cpp,build/%.o,$(patsubst src/%.cpp,build/%.o,$(SOURCE)))

all: nix-analyzer

build/parser-tab.cpp build/parser-tab.hh: src/parser.y
	mkdir -p build
	bison -v -o build/parser-tab.cpp $< -d

build/lexer-tab.cpp build/lexer-tab.hh: src/lexer.l
	mkdir -p build
	flex --outfile build/lexer-tab.cpp --header-file=build/lexer-tab.hh $<

build/%.o: src/%.cpp
	mkdir -p build
	g++ $(CFLAGS) $< -c -o $@

build/%.o: build/%.cpp
	mkdir -p build
	g++ $(CFLAGS) $< -c -o $@

nix-analyzer: $(OBJ)
	g++ $(CFLAGS) $(OBJ) -o nix-analyzer

clean:
	rm -rf build
	rm nix-analyzer