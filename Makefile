CFLAGS:=-Isrc -Ibuild -O3 -Wall -std=c++20 $(shell pkg-config --cflags --libs nix-main bdw-gc nlohmann_json) -I$(boostInclude) -DNIX_VERSION=\"$(NIX_VERSION)\" -lnixutil -lnixstore -lnixexpr -lnixmain -lnixcmd -lnixfetchers -lgc
SOURCE:=$(wildcard src/*.cpp) build/lexer-tab.cc build/parser-tab.cc
OBJ:=$(patsubst build/%.cc,build/%.o,$(patsubst src/%.cpp,build/%.o,$(SOURCE)))

all: nix-analyzer

build/parser-tab.cc build/parser-tab.hh: src/parser.y
	mkdir -p build
	bison -v -o build/parser-tab.cc $< -d

build/lexer-tab.cc build/lexer-tab.hh: src/lexer.l
	mkdir -p build
	flex --outfile build/lexer-tab.cc --header-file=build/lexer-tab.hh $<

build/%.o: src/%.cpp
	mkdir -p build
	g++ $(CFLAGS) $< -c -o $@

build/%.o: build/%.cc
	mkdir -p build
	g++ $(CFLAGS) $< -c -o $@

nix-analyzer: $(OBJ)
	g++ $(CFLAGS) $(OBJ) -o nix-analyzer

clean:
	rm -rf build
	rm nix-analyzer